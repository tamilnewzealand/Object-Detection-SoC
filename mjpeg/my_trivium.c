#include "my_trivium.h"
#include "trivium.c"

// A mess of includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <math.h>
#include <time.h>

#define soc_cv_av
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "arm_a9_hps_0.h"
#include "hwlib.h"

#define HW_REGS_BASE ( ALT_STM_OFST )
#define HW_REGS_SPAN ( 0x04000000 )
#define HW_REGS_MASK ( HW_REGS_SPAN - 1 )

#define USE_FPGA 1

// Optionally reverse the byte order for displaying the key and iv
// Since the processor will reverse the order displaying it this way makes it
//	easier to copy and paste into vhdl with bit order 80 DOWNTO 1
#define REVERSE 1

void print_key(const u8 key[]) {
	printf("Key: ");
	#if !REVERSE
		for (int i=0; i<MAXKEYSIZEB; ++i) {
			printf("%02x", key[i]);
		}
	#else
		for (int i=MAXKEYSIZEB; i; --i) {
			printf("%02x", key[i-1]);
		}
	#endif
	printf("\n");
}

void print_iv(const u8 iv[]) {
	printf("IV: ");
	#if !REVERSE
		for (int i=0; i<MAXIVSIZEB; ++i) {
			printf("%02x", iv[i]);
		}
	#else
		for (int i=MAXIVSIZEB; i; --i) {
			printf("%02x", iv[i-1]);
		}
	#endif
	printf("\n");
}

int test_trivium(void) {
    #define LEN 32
    ECRYPT_ctx ctx;
    const u8 key[MAXKEYSIZEB] = "Test key01";
    const u8 iv[MAXIVSIZEB] = "So Random\0";
    const u8 input[LEN] = "#include \"ecrypt-sync.h\"\r\n#inclu";
    u8 output[LEN];
    u8 second[LEN];
    int passed = 1;
    int i;

    if (USE_FPGA == 1) {
		int fd;
		if (( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
			printf( "ERROR: could not open \"/dev/mem\"...\n" );
		}

		// Map hardware registers into memory
		void *virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE );

		if (virtual_base == MAP_FAILED) {
			printf("ERROR: mmap() failed...\n");
			close(fd);
		}

		u8 temp[20];
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 20; j++) { // Copy corresponding 20 bits
				temp[j] = key[j+i*20];
			}
			// Control signal = 8+i, data value = temp
		}
		
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 20; j++) { // Copy corresponding 20 bits
				temp[j] = iv[j+i*20];
			}
			// Control signal = 12+i, data value = temp
		}

		// Control signal = 0001, data value = 0000 0000 0000 0000 0000
		// Wait 1 s
		
		for (int i = 0; i < 32; i++ ) { // Each character of input
			for (int j = 0; j < 8; j++) { // Each bit of character
				if (input[i] & (1UL << j) == 0x01) {
					// Control signal = 0011, data value = 0000 0000 0000 0000 0001
				} else {
					// Control signal = 0011, data value = 0000 0000 0000 0000 0000
				}
			}
		}
		
		#ifdef LEDS_COMPONENT_NAME
			printf("Found an LED component!\n");

			void *h2p_lw_led_addr = virtual_base + ((unsigned long)(ALT_LWFPGASLVS_OFST + LEDS_BASE) & (unsigned long)(HW_REGS_MASK));

			const int led_mask = (1 << (LEDS_DATA_WIDTH)) - 1; // e.g. 

			*(uint32_t *)h2p_lw_led_addr = 0xff55 & led_mask;
		#else
			printf("These aren't the LEDs you're looking for\n");
		#endif
		
		if (munmap(virtual_base, HW_REGS_SPAN) != 0) {
			printf("ERROR: munmap() failed...\n");
			close(fd);
		}

		close(fd);	

	} else {	
		ECRYPT_init();

		ECRYPT_ivsetup(&ctx, iv);
		ECRYPT_encrypt_blocks(&ctx, input, output, (LEN/ECRYPT_BLOCKLENGTH));

		ECRYPT_ivsetup(&ctx, iv);
		ECRYPT_encrypt_blocks(&ctx, output, second, (LEN/ECRYPT_BLOCKLENGTH));
	}

    // Output results of encryption/decryption and test
	printf("Testing...\n");
	print_key(key);
	print_iv(iv);
	printf("\n");

	for (i=0; i<LEN; ++i) {
		passed &= (second[i] == input[i]);
		printf("%02x", input[i]);
	}
	printf("\n\t\t\tv v v v v v v v\n");
	for (i=0; i<LEN; ++i) {
		printf("%02x", output[i]);
	}
	printf("\n\t\t\tv v v v v v v v\n");
	for (i=0; i<LEN; ++i) {
		printf("%02x", second[i]);
	}
	printf("\n");

    // Display test result
	if (passed) {
		printf("PASSED\n");
	} else {
		printf("FAILED\n");
	}

    return passed;
}

int trivium_file(char *encrypted, char *decrypted, const u8 key[], const u8 iv[]) {
    //Note that the cipher is reversible, so passing in a decrypted file as the first argument will
    //  result in an encrypted output

    ECRYPT_ctx ctx;

    // Initialise cipher
    ECRYPT_init();
    ECRYPT_ivsetup(&ctx, iv);

    #define CHUNK 1024
    char buf[CHUNK];
    char outbuf[CHUNK];
    FILE *infile, *outfile;
    size_t nread;

    // Open file streams
    infile = fopen(encrypted, "rb");
    outfile = fopen(decrypted, "w+");
    if (infile && outfile) {

        while ((nread=fread(buf, 1, sizeof(buf), infile)) > 0) {
            ECRYPT_encrypt_blocks(&ctx, (u8*)buf, (u8*)outbuf, (CHUNK/ECRYPT_BLOCKLENGTH));
            if (nread%4) {
                size_t start = (CHUNK/ECRYPT_BLOCKLENGTH) * ECRYPT_BLOCKLENGTH;
                ECRYPT_encrypt_bytes(&ctx, (u8*)(buf+start), (u8*)(outbuf+start), nread%4);
            }
            fwrite(outbuf, 1, nread, outfile);
        }
        if (ferror(infile) || ferror(outfile)) {
            /* deal with error */
        }
        fclose(infile);
        fclose(outfile);

        return 0;
    }
	return 1;
}

int fd;
int trivium_setup(void) {
    // Hint: This would be a good place to setup your memory mappings
	#ifdef ARM
	if((fd = open( "/dev/mem", ( O_RDWR | O_SYNC ))) == -1) {
		printf("ERROR: could not open \"/dev/mem\"...\n");
		return(1);
	}
	#endif

    return 0;
}

int trivium_finish(void) {
	// And now clean up all the memory mappings
    close(fd);
    return 0;
}

int trivium_decrypt_file(char* infile, char* outfile, const u8 key[], const u8 iv[]) {
    trivium_setup();

    // Execute code
    int ret = trivium_file(infile, outfile, key, iv);

    trivium_finish();
    return ret;
}

int trivium_test_cipher(void) {
    trivium_setup();
	
	// Test for input reversible cipher
    int ret = test_trivium();

    trivium_finish();
    return ret;
}
