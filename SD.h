#ifndef _SD_H_
#define _SD_H_

extern bool SDInit(void);
extern bool SDRead(uint32_t u32Sector,
				   uint8_t *pu8Buffer,
				   uint32_t u32SectorCount);
extern bool SDWrite(uint32_t u32Sector,
					uint8_t *pu8Buffer,
					uint32_t u32SectorCount);
extern bool SDGetSectorCount(uint32_t *pu32SectorCount);
extern bool SDGetBlockSize(uint32_t *pu32BlockSize);

#endif