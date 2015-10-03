/*  test_ndx.cpp -- Console app for testing the B-tree indexing routines
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#define _CRT_SECURE_NO_DEPRECATE 

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <nub/Index.h>

#if NUB_PLATFORM == NUB_PLATFORM_LINUX
#include <iostream>
#include <sys/select.h>

bool _kbhit(void)
{
  struct timeval tv;
  fd_set read_fd;

  /* Do not wait at all, not even a microsecond */
  tv.tv_sec=0;
  tv.tv_usec=0;

  /* Must be done first to initialize read_fd */
  FD_ZERO(&read_fd);

  /* Makes select() ask if input is ready:
   * 0 is the file descriptor for stdin    */
  FD_SET(0,&read_fd);

  /* The first parameter is the number of the
   * largest file descriptor to check + 1. */
  if(select(1, &read_fd,NULL, /*No writes*/NULL, /*No exceptions*/&tv) == -1)
    return false;  /* An error occured */

  /*  read_fd now holds a bit map of files that are
   * readable. We test the entry for the standard
   * input (file 0). */
  
	if(FD_ISSET(0,&read_fd))
		/* Character pending on stdin */
		return true;

  /* no characters were pending */
  return false;
}
#define _getch() getchar()

#else
#include <conio.h>
#endif


using namespace nub;

#define defaultFilename "test.ndx"

Index ndx;

void
pret(int ret)
{
    printf("ret = %d, ckey = \"%s\", %u\n", ret, (const char*)ndx.key(), ndx.offset());
}


char randKey[1024];


char*
getRandKey(int len)
{
    randKey[len] = '\0';
    for (int j = 0; j < len; j++)
        randKey[j] = 'a' + rand() % 26;
    return randKey;
}


void
addRandKey(int len)
{
    pret(ndx.insert(getRandKey(len), 1));
}


int
main(int argc, char** argv)
{
    FILE* outf;
    srand(2); // guarentee repeatable results

	const char* filename = defaultFilename;
	if (argc > 1) filename = argv[1];

    if (outf = fopen(filename, "rb")) {
        fclose(outf);
        ndx.open(filename);
    } else
        ndx.create(filename);

	int maxKeyLen = ndx.maxKeySize() - 1;

    printf("NDX Test - maxkeylen = %d\n", maxKeyLen);

    int ret;
    int count, len;

    while (1) {
        printf("\n"
               "A)dd, F)ind, D)elete, R)andom, L)ist, O)utput, B)reak, C)reate,\n"
               ".)Next, ,)Prev, 0)First, 9)Last, V)alidate, T)est, Q)uit, H)elp : ");
        char inp[256];
        if (gets(inp)) 
            switch(toupper(*inp)) {
                case 'H' :
                    printf("\n"
                           "A) Add a key\n"
                           "F) Find a key\n"
                           "D) Delete a key\n"
                           "R) Add Count random keys of Length Len\n"
                           "L) List all keys in the index\n"
                           "O) Output the index tree to ndx.txt\n"
                           "B) Used to enter the debugger.  Only works if you set a breakpoint.\n"
                           "C) Create the index from scratch\n"
                           ".) Next key\n"
                           ",) Previous key\n"
                           "0) First key\n"
                           "9) Last key\n"
						   "V) Validate the index\n"
                           "T) Continuous Test.  Stops on key or invalid index.\n"
                           "Q) Quit\n"
                           "H) Display this help\n");
                    break;
                case 'B' :
					printf("Debug break\n");  // set a breakpoint here
                    break;
                case '0' :
                    pret(ndx.first());
                    break;
                case '9' :
                    pret(ndx.last());
                    break;
                case '.' :
                    pret(ndx.next());
                    break;
                case ',' :
                    pret(ndx.prev());
                    break;
                case 'C' :
                    ndx.close();
                    ndx.create(filename);
					printf("%s created.\n", filename);
                    break;
                case 'A':
                    printf("Add key: ");
                    while (!gets(inp))
                        ;
                    ret = ndx.insert(inp, 1);
                    pret(ret);
                    break;
                case 'F':
                    printf("Find key: ");
                    while (!gets(inp))
                        ;
                    ret = ndx.find(inp);
                    pret(ret);
                    break;
                case 'D':
                    printf("Delete key: ");
                    while (!gets(inp))
                        ;
                    ret = ndx.remove(inp);
                    pret(ret);
                    break;
                case 'R':
                    printf("Add random keys -> Count, Len: ");
					if (scanf("%d, %d", &count, &len) == 2) {
						int i;
						for (i = 1; i <= count; i++) {
                            printf("%5d: ", i);
                            addRandKey(len);
							if (!ndx.valid()) {
								printf("Failed on key %d\n", i);
								break;
							}
                        }
					}
					gets(inp);
                    break;
                case 'L' :
                    if (ndx.first())
                        do
                            printf("%s\n", (const char*)ndx.key());
                        while (ndx.next());
                    break;
                case 'O' :
                    ndx.print("ndx.txt");
					printf("Tree output to ndx.txt\n");
                    break;
                case 'Q' :
                    ndx.close();
                    exit(1);
				case 'V' :
					printf("Validation %s.\n", ndx.valid() ? "successful" : "failed");
					break;
                case 'T' :
					bool bValidate = inp[1] != '-';
					int maxTests = 0;
					if (!bValidate)
						sscanf(inp+2, "%d", &maxTests);
					printf("Test: max test key len [<= %d]: ", maxKeyLen);
					int tLen;
					if (scanf("%d", &tLen) == 1 && tLen > 0 && tLen <= maxKeyLen) {
						gets(inp);
						count = 0;
						bool success = true;
						while (!_kbhit() && (maxTests == 0 || count < maxTests)) {
							if (bValidate && !ndx.valid()) {
								success = false;
								printf("Test failed.  Iteration %d.\n", count);
								break;
							}
							printf("%6d ", ++count);
							switch (rand() % 3) {
								case 0 :
									printf("Add  ");
									getRandKey(1 + rand() % tLen);
									printf("%s ", randKey);
									pret(ndx.insert(randKey, 1));
									break;
								case 1 :
									printf("Find ");
									getRandKey(1 + rand() % tLen);
									printf("%s ", randKey);
									pret(ndx.find(randKey));
									break;
								case 2 :
									printf("Del  ");
									getRandKey(1 + rand() % tLen);
									printf("%s ", randKey);
									if (!ndx.find(randKey))
										printf("not found\n");
									else
										pret(ndx.remove_current());
									break;
							}
						}
						if (success && maxTests == 0) {
							int keycode = _getch();
							if (keycode == 0 || keycode == 0xE0)
								_getch();
						}
					} else
						gets(inp);
                    break;
            }
    }
	return 0;
}
