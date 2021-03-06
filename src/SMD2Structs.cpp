/*
 * SMD2Structs.cpp
 *
 *  Created on: 09.02.2016
 *      Author: Megacrafter127
 */

#include "SMD2Structs.h"
#include "io.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <zlib.h>
#include <stdexcept>

namespace smd2{
	
	block::block(const struct block *copy) {
		memcpy(this,copy,sizeof(struct block));
	}
	block::block(const unsigned int id, const unsigned int hp, const bool active, const unsigned int orientation) {
		this->id=id;
		this->hp=hp;
		this->active=active;
		this->orientation=orientation;
	}
	block::block(const rawBlock *raw, const blocktypeList *list) {
		if(!list) throw std::invalid_argument("the provided list may not be NULL");
		uint32_t buff=0;
		memcpy(&buff+sizeof(uint32_t)-sizeof(rawBlock),raw,sizeof(rawBlock));
		if(buff) {
			buff=ntoh<uint32_t>(buff);
			this->id=buff;
			buff>>=11;
			blockType type=((blockType*)list)[this->id];
			if(type==undefined) {
				throw std::invalid_argument("blocktype not defined");
			}
			this->hp=buff;
			if(type==fullyRotating) {
				this->hp&=0x00FF;
				buff>>=8;
			} else buff>>=9;
			if(type==logic) {
				this->active=buff & 1;
				buff>>=1;
			}
			this->orientation=buff;
		} else {
			this->id=0;
			this->hp=0;
			this->active=false;
			this->orientation=0;
		}
	}
	block::block() {
		this->id=0;
		this->hp=0;
		this->active=false;
		this->orientation=0;
	}
	rawBlock *block::toRaw(rawBlock *target, const blocktypeList *list) const {
		if(!list) throw std::invalid_argument("the provided list may not be NULL");
		blockType type=((blockType*)list)[this->id];
		if(type==undefined) {
			throw std::invalid_argument("blocktype not defined");
		}
		uint32_t buff=this->orientation;
		if(type==logic) {
			buff<<=1;
			if(this->active) buff++;
		}
		if(type==fullyRotating) {
			buff<<=8;
			buff+=(0xFF & this->hp);
		} else {
			buff<<=9;
			buff+=this->hp;
		}
		buff<<=11;
		buff+=id;
		buff=hton<uint32_t>(buff);
		memcpy(target,&buff+sizeof(uint32_t)-sizeof(rawBlock),sizeof(rawBlock));
		return target;
	}
	
	rawChunkData *inflate(rawChunkData *trg,const compressedChunkData *src) {
		uLongf size = sizeof(rawChunkData);
		switch(uncompress(reinterpret_cast<Bytef*>(*trg), &size, *src, sizeof(compressedChunkData))) {
			case Z_OK:
				break;
			case Z_MEM_ERROR:
				// zlib couldn't allocate enough memory
			case Z_BUF_ERROR:
				// the uncompressed data can't fit in the output buffer
			case Z_DATA_ERROR:
				// the compressed data is either uncomplete or corrupted
			default:
				// unknown error code
				return NULL;
		}
		return trg;
	}
	
	compressedChunkData *deflate(compressedChunkData *trg,const rawChunkData *src) {
		uLongf size = sizeof(compressedChunkData);
		switch(compress(*trg, &size, reinterpret_cast<const Byte*>(*src), sizeof(rawChunkData))) {
			case Z_OK:
				break;
			case Z_MEM_ERROR:
				// zlib couldn't allocate enough memory
			case Z_BUF_ERROR:
				// the uncompressed data can't fit in the output buffer
			default:
				// unknown error code
				return NULL;
		}
		return trg;
	}
	
	bool isEmpty(const rawCompressedSegment *segment) {
		size_t *chars=(size_t*)segment;
		for(size_t i=0;i<(sizeof(rawCompressedSegment)/sizeof(size_t));i++) {
			if(chars[i]) return false;
		}
		return true;
	}
	
	segmentHead::segmentHead(const struct segmentHead *copy) {
		memcpy(this,copy,sizeof(struct segmentHead));
	}
	segmentHead::segmentHead(const unsigned char unknownChar, const uint64_t timestamp, const signed long x, const signed long y, const signed long z, const unsigned char type, const unsigned long inlen) {
		this->unknownChar=unknownChar;
		this->timestamp=timestamp;
		this->x=x;
		this->y=y;
		this->z=z;
		this->type=type;
		this->inlen=inlen;
	}
	segmentHead::segmentHead(const rawCompressedSegment *raw,const bool offset) {
		size_t i=0;
		char *chars=(char*)raw;
		if(offset) {
			this->unknownChar=chars[i];
			i++;
		}
		this->timestamp=readFromNetBuffer<uint64_t>(&chars[i]);
		i+=sizeof(uint64_t);
		this->x=readFromNetBuffer<int32_t>(&chars[i]);
		i+=sizeof(int32_t);
		this->y=readFromNetBuffer<int32_t>(&chars[i]);
		i+=sizeof(int32_t);
		this->z=readFromNetBuffer<int32_t>(&chars[i]);
		i+=sizeof(int32_t);
		this->type=chars[i];
		i++;
		this->inlen=readFromNetBuffer<uint32_t>(&chars[i]);
	}
	segmentHead::segmentHead() {
		this->unknownChar=0;
		this->timestamp=0;
		this->x=0;
		this->y=0;
		this->z=0;
		this->type=0;
		this->inlen=0;
	}
	
	rawCompressedSegment *segmentHead::toRaw(rawCompressedSegment *target,const bool offset) const {
		size_t i=0;
		char *chars=(char*)target;
		if(offset) {
			chars[i]=this->unknownChar;
			i++;
		}
		writeToNetBuffer<uint64_t>(&chars[i], this->timestamp);
		i+=sizeof(uint64_t);
		writeToNetBuffer<int32_t>(&chars[i], this->x);
		i+=sizeof(int32_t);
		writeToNetBuffer<int32_t>(&chars[i], this->y);
		i+=sizeof(int32_t);
		writeToNetBuffer<int32_t>(&chars[i], this->z);
		i+=sizeof(int32_t);
		chars[i]=this->type;
		i++;
		writeToNetBuffer<uint32_t>(&chars[i], this->inlen);
		return target;
	}
	
	segment::segment(const struct segment *copy) {
		memcpy(this,copy,sizeof(struct segment));
	}
	segment::segment(const struct segmentHead *head,const chunkData *blocks) {
		this->head=*head;
		memcpy(&(this->blocks),blocks,sizeof(chunkData));
	}
	
	static void inplaceRawChunkToChunk(chunkData& trg,
									   const rawChunkData& src,
									   const blocktypeList* list) {
		for(unsigned int z = 0 ; z < 16 ; ++z)
			for(unsigned int y = 0 ; y < 16 ; ++y)
				for(unsigned int x = 0 ; x < 16 ; ++x)
					trg[z][y][x] = block(&src[z][y][x], list);
	}
	
	segment::segment(const struct segmentHead *head,const rawChunkData *blocks,const blocktypeList *list):
	head(head) {
		inplaceRawChunkToChunk(this->blocks, (*blocks), list);
	}
	
	segment::segment(const struct segmentHead *head,const compressedChunkData *blocks, const blocktypeList* list):
	head(head) {
		rawChunkData rawBlocks;
		if(!inflate(&rawBlocks, blocks))
			throw std::runtime_error("decompression failed");
		inplaceRawChunkToChunk(this->blocks, rawBlocks, list);
	}
	
	segment::segment(const struct rawSegment *src,const blocktypeList *list):
	head(src->head) {
		inplaceRawChunkToChunk(blocks, src->blocks, list);
	}
	
	segment::segment(const struct compressedSegment *src,
					 const blocktypeList *list):
	head(src->head) {
		rawChunkData rawBlocks;
		if(!inflate(&rawBlocks, &src->blocks))
			throw std::runtime_error("decompression failed");
		inplaceRawChunkToChunk(blocks, rawBlocks, list);
	}
	
	static const std::size_t segmentHeaderSize =
	sizeof(rawCompressedSegment) - sizeof(compressedChunkData) - 1;
	
	segment::segment(const rawCompressedSegment *src,
					 const blocktypeList *list):
	head(src, true) {
		compressedChunkData blocks;
		rawChunkData rawBlocks;
		memcpy(blocks, src+segmentHeaderSize + 1, sizeof(compressedChunkData));
		if(!inflate(&rawBlocks, &blocks)) {
			memcpy(blocks, src+segmentHeaderSize, sizeof(compressedChunkData));
			if(!inflate(&rawBlocks, &blocks))
				throw std::runtime_error("decompression failed");
			head = segmentHead(src, false);
		}
		inplaceRawChunkToChunk(this->blocks, rawBlocks, list);
	}
	
	rawCompressedSegment *segment::toRawCompressed(rawCompressedSegment *trg,const blocktypeList* list,const bool offset) const {
		return rawSegment(this, list).toRawCompressed(trg, offset);
	}
	
	rawSegment::rawSegment(const struct rawSegment *copy) {
		memcpy(this,copy,sizeof(struct rawSegment));
	}
	
	rawSegment::rawSegment(const struct segmentHead *head,const rawChunkData *blocks) {
		this->head=*head;
		memcpy(&(this->blocks),blocks,sizeof(rawChunkData));
	}
	
	rawSegment::rawSegment(const struct segmentHead *head,const chunkData* blocks, const blocktypeList* list):
	head(head) {
		for(unsigned int z = 0 ; z < 16 ; ++z)
			for(unsigned int y = 0 ; y < 16 ; ++y)
				for(unsigned int x = 0 ; x < 16 ; ++x)
					(*blocks)[z][y][x].toRaw(&this->blocks[z][y][x], list);
	}
	
	rawSegment::rawSegment(const struct segmentHead *head,const compressedChunkData* blocks):
	head(head) {
		if(!inflate(&this->blocks, blocks))
			throw std::runtime_error("decompression failed");
	}
	
	rawSegment::rawSegment(const struct segment *src, const blocktypeList* list):
	head(src->head) {
		for(unsigned int z = 0 ; z < 16 ; ++z)
			for(unsigned int y = 0 ; y < 16 ; ++y)
				for(unsigned int x = 0 ; x < 16 ; ++x)
					src->blocks[z][y][x].toRaw(&blocks[z][y][x], list);
	}
	
	rawSegment::rawSegment(const struct compressedSegment *src):
	head(src->head) {
		if(!inflate(&this->blocks, &src->blocks))
			throw std::runtime_error("decompression failed");
	}
	
	rawSegment::rawSegment(const rawCompressedSegment *src):
	head(src, true) {
		compressedChunkData blocks;
		memcpy(blocks, src + sizeof(rawCompressedSegment)
			   - sizeof(compressedChunkData),
			   sizeof(compressedChunkData));
		if(!inflate(&this->blocks, &blocks)) {
			memcpy(blocks, src + sizeof(rawCompressedSegment)
				   - sizeof(compressedChunkData) - 1,
				   sizeof(compressedChunkData));
			if(!inflate(&this->blocks, &blocks))
				throw std::runtime_error("decompression failed");
			head = segmentHead(src, false);
		}
	}
	
	rawCompressedSegment *rawSegment::toRawCompressed(rawCompressedSegment *trg, const bool offset) const {
		head.toRaw(trg, offset);
		compressedChunkData zipped;
		if(!deflate(&zipped, &blocks))
			return NULL;
		memcpy(trg + segmentHeaderSize + offset, zipped, sizeof(compressedChunkData));
		return trg;
	}
	
	compressedSegment::compressedSegment(const struct compressedSegment *copy) {
		memcpy(this,copy,sizeof(struct compressedSegment));
	}
	compressedSegment::compressedSegment(const struct segmentHead *head,const compressedChunkData *blocks) {
		this->head=*head;
		memcpy(&(this->blocks),blocks,sizeof(compressedChunkData));
	}
	compressedSegment::compressedSegment(const struct segmentHead *head,const chunkData *blocks, const blocktypeList* list):
	head(head) {
		rawChunkData rawBlocks;
		for(unsigned int z = 0 ; z < 16 ; ++z)
			for(unsigned int y = 0 ; y < 16 ; ++y)
				for(unsigned int x = 0 ; x < 16 ; ++x)
					(*blocks)[z][y][x].toRaw(&rawBlocks[z][y][x], list);
		if(!deflate(&this->blocks, &rawBlocks))
			throw std::runtime_error("compression failed");
	}
	
	compressedSegment::compressedSegment(const struct segmentHead *head,const rawChunkData *blocks):
	head(head) {
		if(!deflate(&this->blocks, blocks))
			throw std::runtime_error("compression failed");
	}
	
	compressedSegment::compressedSegment(const struct segment *src, const blocktypeList* list):
	head(src->head) {
		rawSegment raw(src, list);
		if(!deflate(&this->blocks, &raw.blocks))
			throw std::runtime_error("compression failed");
	}
	
	compressedSegment::compressedSegment(const struct rawSegment *src):
	head(src->head) {
		if(!deflate(&blocks, &src->blocks))
			throw std::runtime_error("compression failed");
	}
	
	compressedSegment::compressedSegment(const rawCompressedSegment *src):
	head(src, true) {
		memcpy(blocks, src + sizeof(rawCompressedSegment)
			   - sizeof(compressedChunkData),
			   sizeof(compressedChunkData));
	}
	
	rawCompressedSegment *compressedSegment::toRawCompressed(rawCompressedSegment *trg,const bool offset) const {
		head.toRaw(trg, offset);
		memcpy(trg + segmentHeaderSize + offset, blocks, sizeof(compressedChunkData));
		return trg;
	}
	
	smd2Index::smd2Index(const struct smd2Index *copy) {
		this->id=copy->id;
		this->inlen=copy->inlen;
	}
	smd2Index::smd2Index(const signed long id, const unsigned long len) {
		this->id=id;
		this->inlen=len;
	}
	smd2Index::smd2Index() {
		this->id=-1;
		this->inlen=0;
	}
	smd2Index::smd2Index(const rawSmd2Index *raw) {
		const uint32_t *split=(uint32_t*)raw;
		this->id=ntoh<uint32_t>(split[0]);
		this->inlen=ntoh<uint32_t>(split[1]);
	}
	rawSmd2Index *smd2Index::toRaw(rawSmd2Index *trg) const {
		uint32_t *split=(uint32_t*)trg;
		split[0]=hton<uint32_t>(this->id);
		split[1]=hton<uint32_t>(this->inlen);
		return trg;
	}
	bool smd2Index::isValid() {
		return this->id!=-1;
	}
	
	smd2Head::smd2Head(const struct smd2Head *copy) {
		memcpy(this,copy,sizeof(struct smd2Head));
	}
	smd2Head::smd2Head(const unsigned long version, const fullSmd2Index *index,const fullSmd2TimestampHead *timestamps) {
		this->version=version;
		memcpy(&(this->index),index,sizeof(fullSmd2Index));
		memcpy(&(this->timestamps),timestamps,sizeof(fullSmd2TimestampHead));
	}
	smd2Head::smd2Head(const rawSmd2Head *ptr) {
		const Bytef *src = *ptr;
		version=readFromNetBuffer<int32_t>(src);
		src += sizeof(int32_t);
		
		const Bytef *end = src + (4096 * sizeof(rawSmd2Index));
		for(smd2Index *trg = reinterpret_cast<smd2Index*>(&index) ;
			src != end ; src += sizeof(rawSmd2Index), ++trg)
			*trg = smd2Index(reinterpret_cast<const rawSmd2Index*>(src));
		
		end += 4096 * sizeof(int64_t);
		for(int64_t *trg = reinterpret_cast<int64_t*>(&timestamps) ;
			src != end ; src += sizeof(int64_t), ++trg)
			*trg = readFromNetBuffer<int64_t>(src);
	} //TODO #26
	
	rawSmd2Head *smd2Head::toRaw(rawSmd2Head *ptr) const {
		Bytef *trg = *ptr;
		writeToNetBuffer<int32_t>(trg, version);
		trg += sizeof(int32_t);
		
		Bytef *end = trg + (4096 * sizeof(rawSmd2Index));
		for(const smd2Index *src = reinterpret_cast<const smd2Index*>(&index) ;
			trg != end ; trg += sizeof(rawSmd2Index), ++src)
			src->toRaw(reinterpret_cast<rawSmd2Index*>(trg));
		
		end += 4096 * sizeof(int64_t);
		for(const int64_t *src = reinterpret_cast<const int64_t*>(&timestamps) ;
			trg != end ; trg += sizeof(rawSmd2Index), ++src)
			writeToNetBuffer<int64_t>(trg, *src);
		return ptr;
	} //TODO #27
	
	unsigned int getSegmentSlotCountFromSMD2Size(const size_t size) {
		return (size-sizeof(rawSmd2Head))/sizeof(rawCompressedSegment);
	}
	size_t getSMD2SizeFromSegmentSlotCount(const unsigned int slots) {
		return sizeof(rawSmd2Head)+(slots*sizeof(rawCompressedSegment));
	}
}


