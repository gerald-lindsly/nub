/*  test_res.cpp -- Console app for testing resource files
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/

#include <nub/ResourceFile.h>

// TODO: Not much of a test here yet.  Needs work.
// Just now an example on using getStream

const char* resFileName = "test_res";      // the .0 & .1 resource files
const char* resName     = "test_res.cpp";  // the file to store and retrieve

int main()
{
    nub::ResourceFile res;

	char filename[1024];
	strcpy(filename, resFileName);
	strcat(filename, ".0");

	FILE* fExists;
    if (fExists = fopen(filename, "rb")) {
        fclose(fExists);
        res.open(resFileName);
	} else {
        res.open(resFileName, true);
		if (!res.putFile(resName)) {
			printf("File not found: %s", resName);
			return 1;
		}
    };

    std::istream* is = res.getStream(resName);
    const int bufSize = 128;
    char s[bufSize+1];
    do {
        is->read(s, bufSize);
        s[is->gcount()] = 0;
        fputs(s, stdout);
    } while (is->gcount());

    delete is;
    return 0;
}
