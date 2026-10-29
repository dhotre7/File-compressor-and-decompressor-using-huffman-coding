#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "compressor.h"

typedef struct node {
  unsigned char ch;
  unsigned long freq;
  struct node *left;
  struct node *right;
} node;

typedef struct heap {
  node *data[256];
  int size;
} heap;

typedef struct table {
  unsigned char ch;
  int freq;
} table;

typedef struct bitwriter {
  int fd;
  unsigned char cur;
  int nbits;
} bitwriter;

// sets the heap size to zero so the heap starts empty
static void init_heap(heap *h) { h->size = 0; }

// moves a node down until both of its children have a bigger frequency
static void heapify(heap *h, int i) {
  int l = 2 * i + 1;
  int r = 2 * i + 2;
  int smallest = i;
  if (l < h->size && h->data[l]->freq < h->data[smallest]->freq) {
    smallest = l;
  }
  if (r < h->size && h->data[r]->freq < h->data[smallest]->freq) {
    smallest = r;
  }
  if (smallest != i) {
    node *temp = h->data[i];
    h->data[i] = h->data[smallest];
    h->data[smallest] = temp;
    heapify(h, smallest);
  }
}

// removes and returns the node with the smallest frequency
static node *extract_min(heap *h) {
  if (h->size == 0) {
    return NULL;
  }
  node *min = h->data[0];
  h->data[0] = h->data[h->size - 1];
  h->size--;
  heapify(h, 0);
  return min;
}

// creates a new tree node with no children
static node *create_node(unsigned char ch, unsigned long freq) {
  node *new_node = (node *)malloc(sizeof(node));
  new_node->ch = ch;
  new_node->freq = freq;
  new_node->left = NULL;
  new_node->right = NULL;
  return new_node;
}

// adds a node at the end of the heap and moves it up to its place
static void insert_heap(heap *h, node *n) {
  int j = h->size;
  h->data[j] = n;
  h->size++;

  while (j > 0) {
    int p = (j - 1) / 2;
    if (h->data[j]->freq < h->data[p]->freq) {
      node *temp = h->data[j];
      h->data[j] = h->data[p];
      h->data[p] = temp;
      j = p;
    } else {
      break;
    }
  }
}

// walks the tree and stores the 0/1 code of every leaf in codes
static void generate_codes(node *root, char *path, int depth, char codes[256][256]) {
  if (root == NULL) {
    return;
  }
  if (root->left == NULL && root->right == NULL) {
    if (depth == 0) {
      path[0] = '0';
      depth = 1;
    }
    path[depth] = '\0';
    strcpy(codes[root->ch], path);
    return;
  }
  path[depth] = '0';
  generate_codes(root->left, path, depth + 1, codes);
  path[depth] = '1';
  generate_codes(root->right, path, depth + 1, codes);
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

// adds one bit to the output and writes a byte once 8 bits are collected
static void put_bit(bitwriter *w, int bit) {
  w->cur = (w->cur << 1) | bit;
  w->nbits++;
  if (w->nbits == 8) {
    if (write(w->fd, &w->cur, 1) != 1) {
      printf("Error writing file\n");
      exit(1);
    }
    w->cur = 0;
    w->nbits = 0;
  }
}

// writes the lowest n bits of value, highest bit first
static void put_bits(bitwriter *w, unsigned int value, int n) {
  for (int i = n - 1; i >= 0; i--) {
    put_bit(w, (value >> i) & 1);
  }
}

// fills the last byte with zero bits and writes it
static void flush_bits(bitwriter *w) {
  while (w->nbits != 0) {
    put_bit(w, 0);
  }
}

// writes the tree shape: 1 and the byte for a leaf, 0 and both subtrees for any other node
static void write_tree(bitwriter *w, node *root) {
  if (root->left == NULL && root->right == NULL) {
    put_bit(w, 1);
    put_bits(w, root->ch, 8);
    return;
  }
  put_bit(w, 0);
  write_tree(w, root->left);
  write_tree(w, root->right);
}

// compresses the input file into the output file using huffman coding
int compress(char *filename, char *outname) {
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

  int arr[256];
  memset(arr, 0, sizeof(arr));
  unsigned char ch;
  while (read(fd, &ch, 1) > 0) {
    arr[ch]++;
  }
  int count = 0;
  for (int i = 0; i < 256; i++) {
    if (arr[i] > 0) {
      count++;
    }
  }
  if (count == 0) {
    close(fd);
    close(out);
    return 0;
  }
  table char_table[count];
  int index = 0;
  for (int i = 0; i < 256; i++) {
    if (arr[i] > 0) {
      char_table[index].ch = i;
      char_table[index].freq = arr[i];
      index++;
    }
  }
  heap h;
  init_heap(&h);
  for (int i = 0; i < count; i++) {
    insert_heap(&h, create_node(char_table[i].ch, char_table[i].freq));
  }

  while (h.size > 1) {
    node *a = extract_min(&h);
    node *b = extract_min(&h);
    node *new_node = create_node(0, a->freq + b->freq);
    new_node->left = a;
    new_node->right = b;
    insert_heap(&h, new_node);
  }
  node *root = extract_min(&h);

  char codes[256][256];
  memset(codes, 0, sizeof(codes));
  char path[256];
  generate_codes(root, path, 0, codes);

  unsigned long original_size = 0;
  unsigned long total_bits = 3 + 10 * count - 1;
  for (int i = 0; i < 256; i++) {
    original_size += arr[i];
    total_bits += (unsigned long)arr[i] * strlen(codes[i]);
  }
  int padding = (8 - total_bits % 8) % 8;

  bitwriter w;
  w.fd = out;
  w.cur = 0;
  w.nbits = 0;
  put_bits(&w, padding, 3);
  write_tree(&w, root);

  lseek(fd, 0, SEEK_SET);
  while (read(fd, &ch, 1) > 0) {
    for (int i = 0; codes[ch][i] != '\0'; i++) {
      put_bit(&w, codes[ch][i] == '1');
    }
  }
  flush_bits(&w);

  printf("Original size: %lu bytes\n", original_size);
  printf("Compressed size: %lu bytes\n", (total_bits + padding) / 8);

  free_tree(root);
  close(fd);
  close(out);
  return 0;
}

#ifdef STANDALONE
// standalone entry point for compressor
int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <input file> <output file>\n", argv[0]);
    exit(1);
  }
  return compress(argv[1], argv[2]);
}
#endif
