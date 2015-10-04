#include <nub/Platform.h>
#include <stdio.h>

using namespace nub;

#define CHECK_SIZE(s, t, n) { if (sizeof(t) != n) { err = true; printf("error: sizeof(%s) != %d", s, n); } }

int main() {
	bool err = false;
	CHECK_SIZE("int32",  int32,  4);
	CHECK_SIZE("int16",  int16,  2);
	CHECK_SIZE("int8",   int8,   1);
	CHECK_SIZE("uint32", uint32, 4);
	CHECK_SIZE("uint16", uint16, 2);
	CHECK_SIZE("uint8",  uint8,  1);
	CHECK_SIZE("int64",  int64,  8);
	if (!err)
		puts("No error");
	else
		getchar();
	return (int) err;
}