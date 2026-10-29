#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "decompressor.h"

typedef struct node {
  unsigned char ch;
  struct node *left;
  struct node *right;
} node;

typedef struct bitreader {
  int fd;
  unsigned char cur;
  int nbits;
  unsigned long bit_count;
} bitreader;

// creates a new tree node
static node *create_node(unsigned char ch) {
  node *new_node = (node *)malloc(sizeof(node));
  new_node->ch = ch;
  new_node->left = NULL;
  new_node->right = NULL;
  return new_node;
}

// frees every node of the tree
static void free_tree(node *root) {
  if (root == NULL) {
    return;
  }
  free_tree(root->left);
  free_tree(root->right);
  free(root);
}

// reads one bit from the input file
static int get_bit(bitreader *r) {
  if (r->nbits == 0) {
    read(r->fd, &r->cur, 1);
    r->nbits = 8;
  }
  r->nbits--;
  r->bit_count++;
  return (r->cur >> r->nbits) & 1;
}

// reads the next n bits and returns the value
static unsigned int get_bits(bitreader *r, int n) {
  unsigned int value = 0;
  for (int i = 0; i < n; i++) {
    value = (value << 1) | get_bit(r);
  }
  return value;
}

// rebuilds the huffman tree from the bitstream
static node *read_tree(bitreader *r) {
  if (get_bit(r) == 1) {
    return create_node(get_bits(r, 8));
  }
  node *new_node = create_node(0);
  new_node->left = read_tree(r);
  new_node->right = read_tree(r);
  return new_node;
}

// decompresses the input file into the output file using huffman coding
int decompress(char *filename, char *outname) {
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    printf("Error opening file\n");
    return 1;
  }
  int out = open(outname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out == -1) {
    printf("Error creating output file\n");
    close(fd);
    return 1;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == 0) {
    close(fd);
    close(out);
    return 0;
  }
  lseek(fd, 0, SEEK_SET);

  bitreader r;
  r.fd = fd;
  r.cur = 0;
  r.nbits = 0;
  r.bit_count = 0;

  int padding = get_bits(&r, 3);
  unsigned long total_bits = (unsigned long)size * 8 - padding;

  node *root = read_tree(&r);

  if (root->left == NULL && root->right == NULL) {
    while (r.bit_count < total_bits) {
      get_bit(&r);
      write(out, &root->ch, 1);
    }
  } else {
    node *curr = root;
    while (r.bit_count < total_bits) {
      int bit = get_bit(&r);
      if (bit == 0) {
        curr = curr->left;
      } else {
        curr = curr->right;
      }
      if (curr->left == NULL && curr->right == NULL) {
        write(out, &curr->ch, 1);
        curr = root;
      }
    }
  }

  printf("Decompressed successfully\n");

  free_tree(root);
  close(fd);
  close(out);
  return 0;
}

#ifdef STANDALONE
// standalone entry point for decompressor
int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <input file> <output file>\n", argv[0]);
    exit(1);
  }
  return decompress(argv[1], argv[2]);
}
#endif
