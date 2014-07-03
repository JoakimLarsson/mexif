/*
 * Mexif - a minmal and fast exif utility
 *
 * Author: Joakim Larsson, joakim@bildrulle.nu
 * Version: 1.0b
 * Copyright (c) 2007-2014, Joakim Larsson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the originating download site nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY <copyright holder> ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const char leth[]  = {0x49, 0x49, 0x2a, 0x00}; // Little endian TIFF header
const char beth[]  = {0x4d, 0x4d, 0x00, 0x2a}; // Big endian TIFF header
const char types[] = {0x00, 0x01, 0x01, 0x02, 0x04, 0x08, 0x00, 0x08, 0x00, 0x04, 0x08}; // size in bytes for EXIF types
 
#define DE_ENDIAN16(val) endian == G_BIG_ENDIAN ? GUINT16_FROM_BE(val) : GUINT16_FROM_LE(val)
#define DE_ENDIAN32(val) endian == G_BIG_ENDIAN ? GUINT32_FROM_BE(val) : GUINT32_FROM_LE(val)
 
#define ENDIAN16_IT(val) endian == G_BIG_ENDIAN ? GUINT16_TO_BE(val) : GUINT16_TO_LE(val)
#define ENDIAN32_IT(val) endian == G_BIG_ENDIAN ? GUINT32_TO_BE(val) : GUINT32_TO_LE(val)
 
#define IFD_OFFSET_PUSH(val) if (oi < sizeof(offsets) - 1) offsets[oi++] = val; else return PATCH_EXIF_TOO_MANY_IFDS;
#define IFD_OFFSET_PULL()    offsets[--oi]
 
#define IFD_NAME_PUSH(val) names[ni++] = val;
#define IFD_NAME_PULL()    names[--ni]
 
#define DEBUG(x) //x
 
int minimal_exif_tag_write (const char *filename,
 	                    ExifTag     etag,
 			    void       *data,
 			    int         size,
 			    int         ifds)
{
	/* This function updates ONLY the affected tag. Unlike libexif, it does
	   not attempt to correct or re-format other data. This helps preserve
	   the integrity of certain MakerNote entries, by avoiding re-positioning
	   of the data whenever possible. Some MakerNotes incorporate absolute
	   offsets and others use relative offsets. The offset issue is 
	   avoided if tags are not moved or resized if not required. */

 	FILE *jf;            	// File descriptor to file to patch
 	char  buf[1024 * 64]; 	// Working buffer
 	int   i;               	// index into working buffer
 	int   tag;             	// endianed version of 'etag' in call
 	int   gpsifd;          	// endianed gps ifd pointer tag
 	int   exififd;         	// endianed exif ifd pointer tag
 	int   offset;          	// de-endianed offset in various situations
 	int   tags;            	// number of tags in current ifd
 	int   type;            	// de-endianed type of tag used as index into types[]
 	int   count;           	// de-endianed count of elements in a tag
 	int   stitch;          	// offset in data buffer in type size increments
        int   tiff = 0;   	// offset to active tiff header
        int   endian = 0;   	// detected endian of data
 	int   readsize = 0;   	// number of read bytes from file into buffer
 	int   writesize = 0;   	// number of written bytes from buffer to file
 	int   patches = 0;   	// number of values changed
 
 	// IFD stack variables
 	unsigned long offsets[32];  // Offsets in working buffer to start of IFDs
        char         *names[32];    // Printable names of IFD:s (for debug mostly)
        int           oi = 0;       // index into offsets
        int           ni = 0;       // iundex into names
 	int           cifdi = 0;    // curret ifd index
 
 	DEBUG(printf("minimal_exif_tag_write(%s, %04x, %08x, %d, %02x)\n", filename, etag, data, size, ifds);)
 
        // Init IFD stack
 	IFD_OFFSET_PUSH(0);
 	IFD_NAME_PUSH("START");
 
	g_assert (is_local_file (filename));

 	// get path to file
 	if ((filename = get_file_path_from_uri (filename)) == NULL )
 		return PATCH_EXIF_FILE_ERROR;
 
 	// open file r (read (r))
 	if ((jf = fopen(filename, "r")) == NULL)
 		return PATCH_EXIF_FILE_ERROR;
 
        // Fill buffer
        readsize = fread(buf, 1, sizeof(buf), jf);
 
 	// close file during prcessing
 	fclose(jf);
 
        // Check for TIFF header and catch endianess
 	i = 0;
 	while (i < readsize){
 
 		// Little endian TIFF header
 		if (bcmp(&buf[i], leth, 4) == 0){ 
 			endian = G_LITTLE_ENDIAN;
                }
 
 		// Big endian TIFF header
 		else if (bcmp(&buf[i], beth, 4) == 0){ 
 			endian = G_BIG_ENDIAN;
                }
 
 		// Keep looking through buffer
 		else {
 			i++;
 			continue;
 		}
 		// We have found either big or little endian TIFF header
 		tiff    = i;
 		break;
        }
 
 	// So did we find a TIFF header or did we just hit end of buffer?
 	if (tiff == 0) return PATCH_EXIF_NO_TIFF;
 
        // Endian some tag values that we will look for
 	exififd = ENDIAN16_IT(0x8769);
 	gpsifd  = ENDIAN16_IT(0x8825);
        tag     = ENDIAN16_IT(etag);
 
        // Read out the offset
        offset  = DE_ENDIAN32(*((unsigned long*)(&buf[i] + 4)));
 	i       = i + offset;
 
        // Start out with IFD0 (and add more IFDs while we go)
 	IFD_OFFSET_PUSH(i);
 	IFD_NAME_PUSH("IFD0");
 
        // As long as we find more IFDs check out the tags in each
 	while ((oi >=0 && (i = IFD_OFFSET_PULL()) != 0 && i < readsize - 2)){
 	  
 		cifdi    = oi; // remember which ifd we are at
 
 		DEBUG(printf("%s:\n", IFD_NAME_PULL());)
 		tags    = DE_ENDIAN16(*((unsigned short*)(&buf[i])));
 		i       = i + 2;
 
 		// Check this IFD for tags of interest
 		while (tags-- && i < readsize - 12 ){
 			type   = DE_ENDIAN16(*((unsigned short*)(&buf[i + 2])));
 			count  = DE_ENDIAN32(*((unsigned long*) (&buf[i + 4])));
 			offset = DE_ENDIAN32(*((unsigned long*) (&buf[i + 8])));
 
 			DEBUG(printf("TAG: %04x type:%02d count:%02d offset:%04lx ", 
 				     DE_ENDIAN16(*((unsigned short*)(&buf[i]))), type, count, offset);)
 
 			// Our tag?
 			if (bcmp(&buf[i], (char *)&tag, 2) == 0){ 
 
 				DEBUG(printf("*");)
 				
 				// Local value that can be patched directly in TAG table 
 				if ((types[type] * count) <= 4){
 
 					// Fake TIFF offset
 					offset = i + 8 - tiff; 
 					patches++;
 				}
 				// Offseted value of same or larger size that we can patch
 				else if (types[type] * count >= size){
 
 					// Adjust count to new length
 					count = size / types[type]; 
 					*((unsigned short*)(&buf[i + 4])) = ENDIAN32_IT(count);
 					patches++;
 				}
 				// Otherwise we are not able to patch the new value, at least not here
 				else {
 					fprintf(stderr, "gth_minimal_exif_tag_write: New TAG value does not fit, no patch applied\n");
 					continue;
 				}
				
				if (offset + tiff + count * types[type] > sizeof(buf)) return PATCH_EXIF_TRASHED_IFD;

 				// Copy the data
 				stitch = 0;			       
 				while (stitch < count){
 					switch (types[type]){
 					case 1:
 						buf[tiff + offset + stitch] = *(char *) (data + stitch);
 						break;
 					case 2:
 						*((unsigned short*)(&buf[tiff + offset + stitch * 2])) = 
 							ENDIAN16_IT(*(guint16 *) (data + stitch * 2));
 						break;
 					case 4:
 						*((unsigned long*) (&buf[tiff + offset + stitch * 4])) = 
 							ENDIAN32_IT(*(guint32 *) (data + stitch * 4));
 						break;
 					default:
 						fprintf(stderr, "gth_minimal_exif_tag_write:unsupported element size\n");
						return PATCH_EXIF_UNSUPPORTED_TYPE;
 					}
 					stitch++;
 				}
 			}
 			// EXIF pointer tag?
 			else if (bcmp(&buf[i], (char *)&exififd, 2) == 0){ 
 				IFD_OFFSET_PUSH(offset + tiff);
 				IFD_NAME_PUSH("EXIF");
 			}
 			// GPS pointer tag?
 			else if (bcmp(&buf[i], (char *)&gpsifd, 2) == 0){ 
 				IFD_OFFSET_PUSH(offset + tiff);
 				IFD_NAME_PUSH("GPS");
 			}
 
 			DEBUG(printf("\n");)
 					
 			i = i + 12;
 		}
 		// Check for a valid next pointer and assume that to be IFD1 if we just checked IFD0
 		if (cifdi == 1 && i < readsize - 2 && (i = DE_ENDIAN32((*((unsigned long*) (&buf[i]))))) != 0){ 
 			i = i + tiff;
 			IFD_OFFSET_PUSH(i);
 			IFD_NAME_PUSH("IFD1");
 			
 		}
        }

 	// Check if we need to save
 	if (patches == 0)
 		return PATCH_EXIF_NO_TAGS;
 
 	// open file rb+ (read (r) and modify (+), b indicates binary file accces on non-Unix systems
 	if ((jf = fopen(filename, "rb+")) == NULL)
 		return PATCH_EXIF_FILE_ERROR;
 
        // Save changes
 	writesize = fwrite(buf, 1, readsize, jf);
 	fclose(jf);
 
 	return readsize == writesize ? PATCH_EXIF_OK : PATCH_EXIF_FILE_ERROR; 
}


/*
 * Main function...
 */
int main(int argc, char **argv)
{
        FILE *ss;                   /* input file */
unsigned char

	if (argc > 1){
	  if (strncmp(argv[1], "-", 1) == 0){
	    printf("using stdin\n");
	    ss = stdin;
	  }
	  else {
	    if ( (ss = fopen(argv[1], "rb")) != NULL ){
	      struct stat fstat;
	      if (stat(argv[1], &fstat) == 0 && S_ISREG(fstat.st_mode)){
		printf("Opens %s\n", argv[1]);
	      }
	      else{
		printf("File %s is not a regular file\n", argv[1]);
		exit(1);
	      }
	    }
	    else {
	      printf("Can't open file %s\n", argv[1]);
	      exit(1);
	    }
	  }
	  fread(buf, sizeof(buf), 1, ss);
