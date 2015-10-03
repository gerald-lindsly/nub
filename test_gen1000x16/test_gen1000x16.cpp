#include "../include/nub/Index.h"

#include <stdio.h>
#include <stdlib.h>

char randKey[1024];

char* getRandKey(int len) {
  randKey[len] = '\0';
  for (int j = 0; j < len; j++)
    randKey[j] = 'a' + rand() % 26;
  return randKey;
}

int main() {
  nub::Index ndx;
  ndx.create("test_nub.db");

  for (int i = 0; i != 1000; ++i) {
    char* key = getRandKey(16);
    printf("Iteration: %d, key:%s\n", i+1, key);
    ndx.insert(key, 1);
  }

  ndx.close();
}
