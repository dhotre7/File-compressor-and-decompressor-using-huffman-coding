#include <stdio.h>
#include <stdlib.h>
#include "compressor.h"
#include "decompressor.h"

// menu driven main function
int main() {
  int choice;
  char infile[256];
  char outfile[256];

  while (1) {
    printf("\n--- huffman coding menu ---\n");
    printf("1. compress file\n");
    printf("2. decompress file\n");
    printf("3. exit\n");
    printf("enter choice: ");

    if (scanf("%d", &choice) != 1) {
      break;
    }

    if (choice == 1) {
      printf("enter input file path: ");
      scanf("%s", infile);
      printf("enter output compressed file path: ");
      scanf("%s", outfile);
      compress(infile, outfile);
    } else if (choice == 2) {
      printf("enter input compressed file path: ");
      scanf("%s", infile);
      printf("enter output decompressed file path: ");
      scanf("%s", outfile);
      decompress(infile, outfile);
    } else if (choice == 3) {
      break;
    } else {
      printf("invalid choice\n");
    }
  }

  return 0;
}
