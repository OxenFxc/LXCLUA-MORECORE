/**
 * @file crc.h
 * @brief CRC32 calculation functions.
 */

#if !defined(__CRC_H__)
#define __CRC_H__

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Calculates the CRC32 checksum of a buffer.
 * @param data Pointer to the data buffer.
 * @param length Length of the data buffer in bytes.
 * @return The calculated CRC32 checksum.
 */
unsigned int naga_crc32(unsigned char* data, unsigned int length);

/**
 * @brief Calculates the CRC32 checksum of four integers.
 * @param data Pointer to an array of at least 4 unsigned integers.
 * @return The calculated CRC32 checksum.
 */
unsigned int naga_crc32int(unsigned int *data);

/** @brief The CRC32 lookup table. */
extern unsigned int crc_32_tab[];
	
#if defined(__cplusplus)
}
#endif

#endif
