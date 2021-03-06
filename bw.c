#define _GNU_SOURCE

#include "lbw.h"

static bool verbose = false;
unsigned BLOCK_SIZE = SIZE_BLOCK;

////////////////////////////////////
//
// Burrows Wheeler Transformation
//
//////////////////////////////

long block_size;
uint8_t *buff;

int compare (const int *i1, const int *i2) {
  int l1 = block_size - *i1;
  int l2 = block_size - *i2;
  int res;
  /* Add i1 and i2 to shift before comparing */
  res = memcmp (buff + *i1, buff + *i2,
                   l1 < l2 ? l1 : l2);
  if (res == 0)
    return l2 - l1;
  else
    return res;
}

void bw (char *file) {
  int fd = _open (file, 1),
  result_file = _open (RETURN_BW, 2),
  index = _open (INDEX_BW, 2);
  int indexes[BLOCK_SIZE + 1];
  buff = malloc (sizeof (uint8_t) * BLOCK_SIZE);
  while(1) {
    /* Read from file */
    block_size = read (fd, (char *) buff, BLOCK_SIZE);
    if (block_size == 0)
      break;
    /* Write how many bytes we will write, including the marker */
    int l = block_size + 1;
    write (index, (char *) &l, sizeof(int));
    /* We don't copy and shift the string, only pass a pointer */
    int i;
    for (i = 0 ; i <= block_size ; i++)
      indexes[i] = i;

    qsort (indexes, (int)(block_size + 1), sizeof (int),
          (int (*)(const void *, const void *)) compare);


    int first;
    int last;
    for (i = 0 ; i <= block_size ; i++) {
      /* Beginning of the block*/
      if (indexes[i] == 1)
        first = i;
      /* End of the block */
      if (indexes[i] == 0) {
        last = i;
        /* End of string marker */
        char c = '$';
        write (result_file, &c, sizeof (char));
      }
      else
        write (result_file, &buff[indexes[i] - 1], sizeof (char));
    }
    write (index, (char *) &first, sizeof (int));
    write (index, (char *) &last, sizeof (int));
  }
  free (buff);
  close (fd);
  close (result_file);
}

//////////////////////
//
// MTF Encoding
//
//////////////////

void move_to_front () {
  int fd = _open (RETURN_BW, 1);
  uint16_t original = 0;
  int alp_size = pow (2, MTF_SIZE),
    rw_size = (MTF_SIZE == 8)? 1: 2;
  uint16_t *reading_tab = calloc (alp_size, sizeof (uint16_t));
  while (read(fd, &original, rw_size) > 0)
    reading_tab[original] = 1;

  /* Count each letter of the file */
  int count = 0, i;
  uint16_t first;
  for (i = 0; i < alp_size; i++) {
    if (reading_tab[i] != 0) {
      reading_tab[i] = count;
      count++;
      if (count == 1)
        first = i;
    }
  }
  /* Making the dictionnary for MTF */
  list begin = malloc (sizeof (struct list));
  begin->data = first;
  begin->next = NULL;
  list tmp = begin;
  for (i = 0; i < alp_size; i++)
    if (reading_tab[i] != 0) {
      list next = malloc (sizeof (struct list));
      next->data = (uint16_t)i;
      next->next = NULL;
      tmp->next = next;
      tmp = next;
    }
  if (verbose) {
    tmp = begin;
    printf ("There are %d letters in the dictionnary :(", count);
    for (i = 0; i < count; i++) {
      printf ("%u,", tmp->data);
      tmp = tmp->next;
    }
    printf (")\n");
  }

  /* Write the transformed file */
  int dictionnary_file = _open (DICTIONNARY_ENC, 2);
  /* Print dictionnary on file */
  tmp = begin;
  while (tmp != NULL) {
    write (dictionnary_file, &tmp->data, rw_size);
    if (tmp->next != NULL)
      write (dictionnary_file, ":", 1);
    else
      write (dictionnary_file, ";", 1);
    tmp = tmp->next;
  }
  close (dictionnary_file);

  /* Write the transformed file */
  lseek (fd, 0, SEEK_SET);
  int result_file = _open (RETURN_ENC, 2);
  list prev;
  uint16_t new_val;
  while (read(fd, &original, rw_size) > 0) {
    tmp = begin;
    /* Find the index of the letter original in the dictionnary */
    for (i = 0; tmp->data != original; i++) {
      prev = tmp;
      tmp = tmp->next;
    }
    new_val = (uint16_t)i;
    /* Move tmp to the front of the dictionnary */
    if (i != 0) {
      swap(begin, tmp, prev);
      begin = tmp;
    }
    write(result_file, &new_val, rw_size);
  }

  while (begin->next != NULL) {
    tmp = begin->next;
    free (begin);
    begin = tmp;
  }
  free (begin);
  free (reading_tab);
  close (fd);
  close (result_file);
}

////////////////////
//
//   Huffman Tree
//
///////////////

/* Huffman coding */
node_t *create_leaf (node_t *n) {
  node_t *leaf = malloc (sizeof (*leaf));
  leaf->left = NULL;
  leaf->right = NULL;
  leaf->amount = n->amount;
  leaf->data = n->data;
  return leaf;
}
/* Creates a node with subnodes n1 and n2,
 * and overwrites in with the amount of children */
node_t *create_node (node_t *n1, node_t *n2) {
  node_t *node = malloc (sizeof (*node));
  node->amount = n1->amount + n2->amount;
  node->left = n1;
  node->right = n2;
  return node;
}

void get_min_amounts (node_t **cd, int *i1, int *i2) {
  int index_min = 0, index = 0, i, alp_size = pow (2, HUFF_SIZE);
  bool first = false;
  /* Find two valid indexes */
  for (i = 0; i < alp_size; i++)
    if (cd[i]) {
      if (first) {
        index_min = i;
        break;
      }
      else {
        index = i;
        first = true;
      }
    }
  /* Sort them */
  if (cd[index]->amount < cd[index_min]->amount) {
    int tmp = index_min;
    index_min = index;
    index = tmp;
  }
  /* Find, if possible, other index with lower amount */
  for (i++; i < alp_size; i++) {
    if (cd[i] && cd[i]->amount < cd[index]->amount) {
      if (cd[i]->amount < cd[index_min]->amount) {
        index = index_min;
        index_min = i;
      }
      else {
        index = i;
      }
    }
  }
  *i1 = index_min;
  *i2 = index;
}

node_t *huffman_tree (node_t **cd, unsigned amount_left) {
  /* Sub tree construction */
  node_t *root;
  int i1, i2;
  while (1) {
    if (amount_left <= 2)
      break;
    /* Create a node from the 2 lowest frequencies of apparition */
    get_min_amounts (cd, &i1, &i2);
    cd[i2] = create_node (cd[i2], cd[i1]);
    cd[i1] = NULL;
    amount_left --;
  }
  /* Form root with the last nodes */
  get_min_amounts (cd, &i1, &i2);
  root = create_node (cd[i2], cd[i1]);
  return root;
}

uint8_t *deep_transform (uint8_t *encoding, unsigned depth, uint8_t last_bit) {
  /* 8 is the size of uint8_t in bits */
  unsigned index = depth / 8;
  uint8_t *ret = malloc (sizeof (uint8_t) * (index + 2));
  unsigned i;
  for (i = 0; i < index + 1; i++) {
    ret[i] = encoding[i];
  }
  /* Optimization for copying encoding into the array */
  if (last_bit != 2) {
    ret[index] = (encoding[index] << 1) + last_bit;
  }

  return ret;
}

/* Create the Huffman encoding, for a given node
 * If it has a left child,  we pass the child a 0
 * If it has a right child, we pass the child a 1
 */
void huffman_encoding (node_t *node, transform_t *array,
           unsigned depth, uint8_t *encoding) {
  if (node->left || node->right) {
    /* Intern node */
    uint8_t *enc;
    if (node->left) {
      enc = deep_transform (encoding, depth, 0);
      huffman_encoding (node->left, array, depth + 1, enc);
      free (enc);
    }
    if (node->right) {
      enc = deep_transform (encoding, depth, 1);
      huffman_encoding (node->right, array, depth + 1, enc);
      free (enc);
    }
  }
  else {
    /* Leaf */
    array[node->data].depth = depth;
    array[node->data].encoding = deep_transform (encoding, depth, 2);
  }
}

void free_tree (node_t *node) {
  if (node) {
    if (node->left || node->right) {
      if (node->left)
        free_tree (node->left);
      if (node->right)
        free_tree (node->right);
    }
    free (node);
  }
}

/** The same functonning of cy_data, it will copy on a buffer,*
  * and when the buffer is full, it will write it             **/
int cpy_data_huffman (int file, uint16_t *write_buf, uint16_t data, int size_of_data,
                      int nbaw_write_buf) {
  uint16_t *buffer = write_buf;
  int size_buffer = sizeof (uint16_t) * BYTES_SIZE;
  int nbaw_buf = nbaw_write_buf;
  int nb_cpy = 0;
  int nbaw_data = 0;
  nb_cpy = cpy_data ((uint64_t *)buffer, size_buffer, nbaw_buf,
                     (uint64_t)data, size_of_data, nbaw_data);
  nbaw_buf += nb_cpy;
  while (nbaw_buf == size_buffer) {
    write (file, buffer, sizeof (uint16_t));
    *buffer = 0;
    nbaw_buf = 0;
    nbaw_data += nb_cpy;
    if (nbaw_data != size_of_data) {
      nb_cpy = cpy_data ((uint64_t *)buffer, size_buffer, nbaw_buf,
                         (uint64_t)data, size_of_data, nbaw_data);
      nbaw_buf += nb_cpy;
    }
  }
  return nbaw_buf;
}

void huffman () {
  int fd = _open (RETURN_ENC, 1);
  uint16_t data = 0;
  unsigned alp_size = pow (2, HUFF_SIZE);
  node_t **tab = malloc (sizeof (node_t *) * alp_size);
  unsigned i;
  for (i = 0; i < alp_size; i++)
    tab[i] = NULL;
  unsigned nb_letters = 0;
  /* Count the amount of each letter */
  while (read (fd, &data, HUFF_SIZE / BYTES_SIZE) > 0) {
    if (tab[(int)data] == NULL) {
      node_t *new_data = malloc (sizeof (node_t));
      new_data->data = data;
      new_data->amount = 1;
      new_data->left = NULL;
      new_data->right = NULL;
      tab[(int)data] = new_data;
      nb_letters++;
    }
    else
      tab[(int)data]->amount++;
  }
  /* VERBOSE */
  unsigned k = 0;
  for (unsigned j = 0; j < alp_size; j++) {
    if (tab[j] != NULL) {
      if (verbose) {
        printf ("j = %d,   ",j);
        print_array2 (tab[j]->data);
        printf (", amount = %u\n", tab[j]->amount);
      }
      k += tab[j]->amount;
    }
  }
  if (verbose) {
    printf ("\nOut of %u data analyzed", i);
    printf ("\n");
  }

  /* Creation of the Huffman Tree */
  node_t *root = huffman_tree (tab, nb_letters);
  transform_t *array = malloc (sizeof (transform_t ) * alp_size);
  for (i = 0; i < alp_size; i++){
    array[i].depth = 0;
    array[i].encoding = 0;
  }
  uint8_t init_encoding = 0;
  huffman_encoding (root, array, 0, &init_encoding);
  /* VERBOSE */
  if (verbose) {
    /* Print encoding */
    printf ("\nEncoding: <word> to <encoding>\n");
    for (unsigned j = 0; j < alp_size; j++) {
      if (array[j].encoding) {
        printf ("%d : ", j);
        print_array2 (j);
        printf (" to ");
        print_encoding (array[j]);
        printf("    , depth = %d", array[j].depth);
        printf("\n");
      }
    }
  }

  /* Print the code into a file */
  int huff_code_file = _open (ENCODE_HUF, 2);
  uint16_t to_write = 0;
  int nb_bits = 0;
  uint8_t *encoding;
  for (unsigned j = 0; j < alp_size; j++) {
    encoding = array[j].encoding;
    if (encoding) {
      /* Print the word */
      nb_bits = cpy_data_huffman (huff_code_file, &to_write, (uint16_t)j,
                                  HUFF_SIZE, nb_bits);
      /* Print the size of encoding */
      nb_bits = cpy_data_huffman (huff_code_file, &to_write, array[j].depth,
                                  HUFF_SIZE, nb_bits);
      /* Print the encoding */
      int max_size = sizeof (uint8_t) * BYTES_SIZE;
      int nb_bits_encoding = array[j].depth;
      while (nb_bits_encoding != 0) {
        if (nb_bits_encoding > max_size) {
          nb_bits = cpy_data_huffman (huff_code_file, &to_write, *encoding,
                                      max_size, nb_bits);
          nb_bits_encoding -= max_size;
          encoding++;
        }
        else {
          nb_bits = cpy_data_huffman (huff_code_file, &to_write, *encoding,
                                      nb_bits_encoding, nb_bits);
          nb_bits_encoding = 0;
        }
      }
    }
  }
  uint16_t end = 0;
  int to_push_end_of_encode = sizeof (to_write) * BYTES_SIZE;
  cpy_data_huffman (huff_code_file, &to_write, end,
                    to_push_end_of_encode, nb_bits);
  close (huff_code_file);

  /* Write encoded letters into the final file */
  lseek (fd, 0, SEEK_SET);
  int result_file = _open (RETURN_HUF, 2);
  uint16_t data_read = 0;
  uint8_t data_to_write = 0;
  unsigned width = 0;
  uint64_t nb_bits_writing;
  nb_bits_writing = nb_bits_writing ^ nb_bits_writing;
  for (i = 0; read (fd, &data_read, HUFF_SIZE / BYTES_SIZE) > 0; i++) {
    /* Write the encoding */
    unsigned k, l;
    for (k = 0; k < (array[data_read].depth / BYTES_SIZE); k++) {
      for (l = BYTES_SIZE; l != 0; l--) {
        if (!(array[data_read].encoding[k] & (1 << (l - 1))))
          data_to_write = data_to_write << 1;
        else
          data_to_write = (data_to_write << 1) + 1;
        width++;
        if (width == BYTES_SIZE) {
          write (result_file, &data_to_write, sizeof(uint8_t));
          nb_bits_writing += width;
          width = 0;
          data_to_write = 0;
        }
      }
    }
    /* Write the rest of the encoding, if there is */
    if (array[data_read].depth  % BYTES_SIZE) {
      for (l = array[data_read].depth % BYTES_SIZE; l != 0; l--) {
        if (!(array[data_read].encoding[k] & (1 << (l - 1))))
          data_to_write = data_to_write << 1;
        else
          data_to_write = (data_to_write << 1) + 1;
        width++;
        if (width == BYTES_SIZE) {
          write (result_file, &data_to_write, sizeof(uint8_t));
          nb_bits_writing += width;
          width = 0;
          data_to_write = 0;
        }
      }
    }
  }
  /* One last value to write */
  if (width != 0) {
    nb_bits_writing += width;
    while (width != BYTES_SIZE) {
      data_to_write = data_to_write << 1;
      width++;
    }
    write (result_file, &data_to_write, sizeof(uint8_t));
  }
  int numb_write_file = _open (SIZE_HUF, 2);
  write (numb_write_file, &nb_bits_writing, sizeof(uint64_t));

  close (numb_write_file);
  for (i = 0; i < alp_size; i++)
    if (array[i].encoding)
      free(array[i].encoding);
  free (array);
  free_tree (root);
  free (tab);
  close (result_file);
  close (fd);
  if (verbose)
    printf("\nnb_bits_writing = %lu\n", nb_bits_writing);
}
////////////////////
//
//  Main Function
//
///////////////

/* The archive is composed by
 * <size .code_huff> + <.code_huff> + <size .dico_enc> + <.dico_enc>
 * + <size .index_bw>+ <index_bw> + <.size_huff> + <result_huffman>
 */
void archive_compress (char *file) {
  /* Archive is named <file>.bw */
  int len = strlen (file) + strlen (".bw");
  char *archive = malloc (len);
  strcpy (archive, file);
  archive[len - 3] = '.';
  archive[len - 2] = 'b';
  archive[len - 1] = 'w';
  archive[len] = '\0';
  int fd =  _open (archive, 2);

  int code_h = _open (ENCODE_HUF, 1),
    dico_enc = _open (DICTIONNARY_ENC, 1),
    size_huff= _open (SIZE_HUF, 1),
    index_bw = _open (INDEX_BW, 1),
    res_huff = _open (RETURN_HUF, 1);

  int files[5] = {code_h, dico_enc, index_bw, size_huff, res_huff};
  int size;
  char buffer[1];
  for (int i = 0; i < 5; i++) {
    size = 0;
    if (i < 3) {
      size = lseek (files[i], 0, SEEK_END);
      lseek (files[i], 0, SEEK_SET);
      write (fd, &size, 4);
    }
    while (read (files[i], &buffer, 1) > 0)
      write (fd, &buffer, 1);
  }

  unlink (ENCODE_HUF);
  unlink (DICTIONNARY_ENC);
  unlink (SIZE_HUF);
  unlink (INDEX_BW);
  unlink (RETURN_BW);
  unlink (RETURN_ENC);
  unlink (RETURN_HUF);
  close (fd);
  free (archive);
}

int main (int argc, char **argv) {
  int optc;
  static struct option long_opts[] =
  {
    {"verbose",   no_argument,        0,    'v'},
    {"help",      no_argument,        0,    'h'},
    {NULL,        0,                  NULL, 0}
  };
  int nb_optc = 0;
  while ((optc = getopt_long (argc, argv, "vh", long_opts, NULL)) != -1) {
    if (nb_optc == 1)
      optc = 0;
    switch (optc) {
      case 'v': /* Verbose output */
        verbose = true;
        nb_optc++;
        break;

      case 'h': /* Display this help */
        usage(EXIT_SUCCESS, argv[0]);
        break;

      default:
        usage(EXIT_FAILURE, argv[0]);
    }
  }
  /* Catch parameters and use them */
  if (argc - nb_optc != 3) {
    fprintf (stderr, "BW: usage: bw [option] <file> <BLOCK_SIZE>\n");
    exit (EXIT_FAILURE);
  }
  if (argc == 4) {
    BLOCK_SIZE = atoi (argv[3]);
    bw (argv[2]);
  }
  else {
    BLOCK_SIZE = atoi (argv[2]);
    bw (argv[1]);
  }
  move_to_front ();
  huffman ();
  archive_compress (argv[1]);
  return EXIT_SUCCESS;
}
