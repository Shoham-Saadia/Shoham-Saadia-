/************************** Start of Forward.C *************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bitio.h"
#include <math.h>
#include "errhand.h"
#include <assert.h>
char *CompressionName = "Forward coding, with escape codes";
char *Usage           = "infile outfile [ -d ]";

#define END_OF_STREAM     256
#define ESCAPE            257
#define SYMBOL_COUNT      258
#define NODE_TABLE_COUNT  ( ( SYMBOL_COUNT * 2 ) - 1 )
#define ROOT_NODE         0
#define MAX_WEIGHT        0x7fffffffffffffffL // max value
#define TRUE              1
#define FALSE             0

typedef struct tree {
    int leaf[ SYMBOL_COUNT ]; // 258
    int next_free_node;

    struct node {
        unsigned long weight;
        int parent;
        int child_is_leaf;
        int child;
    } nodes[ NODE_TABLE_COUNT ];
} TREE;

int prelude;

TREE Tree;

#ifdef __STDC__

void CompressFile( FILE *input, BIT_FILE *output, int argc, char *argv[] );
void ExpandFile( BIT_FILE *input, FILE *output, int argc, char *argv[] );
void InitializeTree( TREE *tree );
void BuildTree(TREE *tree , unsigned int *frequency_arr);
void EncodeSymbol( TREE *tree, unsigned int c, BIT_FILE *output);
int DecodeSymbol( TREE *tree, BIT_FILE *input );
void UpdateModelUp( TREE *tree, int c );
void WritePrelud(int size,int *frequency_arr , BIT_FILE * output);
void WriteCdelta(int *frequency_arr, BIT_FILE *output);
void IntToCdelta(int num,BIT_FILE *output);
void ReadCdelta(int *frequency_arr, BIT_FILE *input);
int CdeltaToInt(BIT_FILE *input);
unsigned long Revese(unsigned long code, int size);
void swap_nodes( TREE *tree, int i, int j );
void add_new_node( TREE *tree, int c );
void remove_node( TREE *tree , int c );
void clean_node(TREE *tree , int node);
void UpdateModelDown(TREE *tree, int c);


#else

void CompressFile();
void ExpandFile();
void InitializeTree();
void BuildTree();
void EncodeSymbol();
int DecodeSymbol();
void UpdateModelUp();
void swap_nodes();
void add_new_node();
void WritePrelud();
void WriteCdelta();
void IntToCdelta();
unsigned long Revese();
void ReadCdelta();
int CdeltaToInt();
void UpdateModelDown();
void remove_node();
void clean_node();

#endif


void CompressFile( input, output, argc, argv )
FILE *input;
BIT_FILE *output;
int argc;
char *argv[];
{
    unsigned int frequency_arr  [256] = {0} ;
    int c;
    int size = 0;
    int max = 0;
    while ( ( c = getc( input ) ) != EOF ) {
      if (frequency_arr[c] == 0)
        size++;
      frequency_arr[c]++;
      if (max < frequency_arr[c])
          max = frequency_arr[c];
    }
    InitializeTree( &Tree);
    BuildTree(&Tree , frequency_arr);
    WriteCdelta(frequency_arr, output);
    fseek(input, 0, SEEK_SET);   // move input to the start
    unsigned long frequencySum = 0 ;
    for (size_t i = 0; i < 256; i++) {
       frequencySum+=frequency_arr[i];
     }

    while (frequencySum > 0){
        c = getc( input ) ;
        EncodeSymbol( &Tree, c, output);
        UpdateModelDown( &Tree, c );
        frequencySum--;
    }
    int pre = (int)(floor(prelude/8)); // bit to bytes
    printf("\nprelude = %d bytes\n", pre);

    while ( argc-- > 0 ) {
        if ( strcmp( *argv, "-d" ) != 0 )
            printf( "Unused argument: %s\n", *argv );
        argv++;
    }
}

void ExpandFile( input, output, argc, argv )
BIT_FILE *input;
FILE *output;
int argc;
char *argv[];
{
    int c;
    unsigned int frequency_arr [256] = {0};
    ReadCdelta(frequency_arr , input);
    InitializeTree( &Tree);
    BuildTree(&Tree , frequency_arr );
    unsigned long frequencySum = 0 ;
    for (size_t i = 0; i < 256; i++) {
      frequencySum+=frequency_arr[i];
    }

  while(frequencySum > 0){
     c = DecodeSymbol( &Tree, input ) ;
        if ( putc( c, output ) == EOF )
            fatal_error( "Error writing character" );
        UpdateModelDown( &Tree, c );
        frequencySum--;
    }
    while ( argc-- > 0 ) {
        if ( strcmp( *argv, "-d" ) != 0 )
            printf( "Unused argument: %s\n", *argv );
        argv++;
    }
}


void InitializeTree( tree )
TREE *tree;
{
    int i;

    tree->nodes[ ROOT_NODE ].child             = ROOT_NODE + 1;
    tree->nodes[ ROOT_NODE ].child_is_leaf     = FALSE;
    tree->nodes[ ROOT_NODE ].weight            = 2;
    tree->nodes[ ROOT_NODE ].parent            = -1;

    tree->nodes[ ROOT_NODE + 1 ].child         = END_OF_STREAM;
    tree->nodes[ ROOT_NODE + 1 ].child_is_leaf = TRUE;
    tree->nodes[ ROOT_NODE + 1 ].weight        = 1;
    tree->nodes[ ROOT_NODE + 1 ].parent        = ROOT_NODE;
    tree->leaf[ END_OF_STREAM ]                = ROOT_NODE + 1;

    tree->nodes[ ROOT_NODE + 2 ].child         = ESCAPE;
    tree->nodes[ ROOT_NODE + 2 ].child_is_leaf = TRUE;
    tree->nodes[ ROOT_NODE + 2 ].weight        = 1;
    tree->nodes[ ROOT_NODE + 2 ].parent        = ROOT_NODE;
    tree->leaf[ ESCAPE ]                       = ROOT_NODE + 2;

    tree->next_free_node                       = ROOT_NODE + 3;

    for ( i = 0 ; i < END_OF_STREAM ; i++ )
        tree->leaf[ i ] = -1;
}

void BuildTree(tree , frequency_arr)
  TREE *tree;
  unsigned int *frequency_arr;
{
  for (size_t i = 0; i < 256; i++) {
    if (frequency_arr[i] != 0){
        add_new_node( tree, i) ;
        for (size_t j = 0; j < frequency_arr[i]; j++) {
            UpdateModelUp( tree, i );
        }
     }
  }
  UpdateModelDown(&Tree, END_OF_STREAM);
  UpdateModelDown(&Tree, ESCAPE);
}

void EncodeSymbol( tree, c, output )
TREE *tree;
unsigned int c;
BIT_FILE *output;
{
    unsigned long code;
    unsigned long current_bit;
    int code_size;
    int current_node;

    code = 0;
    current_bit = 1;
    code_size = 0;
    current_node = tree->leaf[ c ];
    while ( current_node != ROOT_NODE ) {
        if ( ( current_node & 1 ) == 0 )
            code |= current_bit;
        current_bit <<= 1;
        code_size++;
        current_node = tree->nodes[ current_node ].parent;
    };
    OutputBits( output, code, code_size );
}
void WritePrelud(size, frequency_arr, output )
  int size;
  int *frequency_arr;
   BIT_FILE *output;
{
  unsigned int count = 4 + 5 * size;
  OutputBits( output, size, sizeof(int)*8 );
  for (size_t i = 0; i < 256 ; i++) {
    if (frequency_arr[i] != 0){
      char c = i ;
      OutputBits( output, c, sizeof(char)*8 );
      OutputBits( output, frequency_arr[i], sizeof(int)*8 );
    }
  }
  printf("prelude bytes : %d\n", count );
}

void WriteCdelta(int *frequency_arr, BIT_FILE *output){
  for (size_t i = 0; i < 256; i++) {
      IntToCdelta(frequency_arr[i]+1,output);
  }
}

void IntToCdelta(int num,BIT_FILE *output){
      int len = 0;
      int lengthOfLen = 0;

      unsigned long current_bit = 1;
      unsigned long code = 0 ;
      int code_size = 0;

      len = 1 + floor(log2(num));
      lengthOfLen = 1 + floor(log2(len));

      for (int i = lengthOfLen - 1; i > 0; --i)
      {
          current_bit <<= 1;
          code_size++;
      }
      for (int i = lengthOfLen - 1; i+1 >= 1; --i)
      {
        if (((len >> i) & 1 ) == 1)
          code |= current_bit;
          current_bit <<= 1;
          code_size++;
      }
      for (int i = len - 2; i+1 >= 1; i--)
      {
        if (((num >> i) & 1 ) == 1)
          code |= current_bit;
          current_bit <<= 1;
          code_size++;
      }
      prelude += code_size;
      OutputBits( output, Revese(code,code_size), code_size );
}

unsigned long Revese(unsigned long code, int size){
  unsigned long maskEnd  = 1;
  unsigned long ans =  0 ;
  maskEnd <<= size-1;
  unsigned long maskStart = 1;
  for (size_t i = 0; i < size; i++) {
    if (maskStart&code)
      ans |= maskEnd;
    maskEnd >>= 1 ;
    maskStart <<= 1;
  }
  return ans;
}

void ReadCdelta(int *frequency_arr, BIT_FILE *input){
  for (size_t i = 0; i < 256; i++) {
    frequency_arr[i] = CdeltaToInt(input)-1;
  }
}
int CdeltaToInt(BIT_FILE *input){
  int num = 1;
  int len = 1;
  int lengthOfLen = 0;
  while (!InputBits(input, 1))
      lengthOfLen++;
  for (int i = 0; i < lengthOfLen; i++)
  {
      len <<= 1;
      if (InputBits(input, 1))
          len |= 1;
  }
  for (int i = 0; i < len - 1; i++)
  {
      num <<= 1;
      if ((int)InputBits(input, 1))
          num |= 1;
  }
  return num;
}

int DecodeSymbol( tree, input )
TREE *tree;
BIT_FILE *input;
{
    int current_node;
    int c;

    current_node = ROOT_NODE;
    while ( !tree->nodes[ current_node ].child_is_leaf ) {
        current_node = tree->nodes[ current_node ].child;
        current_node += InputBit( input );
    }
    c = tree->nodes[ current_node ].child;
    return( c );    // return the leaf index
}

void UpdateModelUp( tree, c )
TREE *tree;
int c;
{
    int current_node;
    int new_node;

    assert( tree->nodes[ ROOT_NODE].weight < MAX_WEIGHT );
    current_node = tree->leaf[ c ];
    while ( current_node != -1 ) {
        tree->nodes[ current_node ].weight++;
        for ( new_node = current_node ; new_node > ROOT_NODE ; new_node-- )
            if ( tree->nodes[ new_node - 1 ].weight >=
                 tree->nodes[ current_node ].weight )
                break;
        if ( current_node != new_node ) {
            swap_nodes( tree, current_node, new_node );
            current_node = new_node;
        }
        current_node = tree->nodes[ current_node ].parent;
    }
}


void swap_nodes( tree, i, j )
TREE *tree;
int i;
int j;
{
    struct node temp;

    if ( tree->nodes[ i ].child_is_leaf )
        tree->leaf[ tree->nodes[ i ].child ] = j;
    else {
        tree->nodes[ tree->nodes[ i ].child ].parent = j;
        tree->nodes[ tree->nodes[ i ].child + 1 ].parent = j;
    }
    if ( tree->nodes[ j ].child_is_leaf )
        tree->leaf[ tree->nodes[ j ].child ] = i;
    else {
        tree->nodes[ tree->nodes[ j ].child ].parent = i;
        tree->nodes[ tree->nodes[ j ].child + 1 ].parent = i;
    }
    temp = tree->nodes[ i ];
    tree->nodes[ i ] = tree->nodes[ j ];
    tree->nodes[ i ].parent = temp.parent;
    temp.parent = tree->nodes[ j ].parent;
    tree->nodes[ j ] = temp;
}


void add_new_node( tree, c )
TREE *tree;
int c;
{
    int lightest_node;
    int new_node;
    int zero_weight_node;

    lightest_node = tree->next_free_node - 1; // old nyt
    new_node = tree->next_free_node;
    zero_weight_node = tree->next_free_node + 1;
    tree->next_free_node += 2;

    tree->nodes[ new_node ] = tree->nodes[ lightest_node ];
    tree->nodes[ new_node ].parent = lightest_node;
    tree->leaf[ tree->nodes[ new_node ].child ] = new_node; // nyt move to next_free_node

    tree->nodes[ lightest_node ].child         = new_node;
    tree->nodes[ lightest_node ].child_is_leaf = FALSE;

    tree->nodes[ zero_weight_node ].child           = c;
    tree->nodes[ zero_weight_node ].child_is_leaf   = TRUE;
    tree->nodes[ zero_weight_node ].weight          = 0;
    tree->nodes[ zero_weight_node ].parent          = lightest_node;
    tree->leaf[ c ] = zero_weight_node;
}

/*
* remove_node Takes a char (int c) and takes it out of the tree
*/
void UpdateModelDown( tree, c )
TREE *tree;
int c;
{
    int current_node, new_node;

    current_node = tree->leaf[ c ];
    while ( current_node != -1 ) {
          tree->nodes[ current_node ].weight--;

        for ( new_node = current_node ; new_node < tree->next_free_node  ; new_node++ ){
            if ( tree->nodes[ new_node + 1 ].weight <=
                 tree->nodes[ current_node ].weight )
                    break;
        }

        if ( current_node != new_node ) {
            swap_nodes( tree, current_node, new_node );
            current_node = new_node;
        }
        current_node = tree->nodes[ current_node ].parent;
    }
      if (tree->nodes[ tree->leaf[ c ] ].weight == 0 ){
        remove_node( tree , c );
      }
}

void remove_node( tree , c )
  TREE *tree ;
  int c ;
{
    int parent_node, brother_node ;
    parent_node = tree->nodes[tree->leaf[c]].parent;
    if (parent_node != ROOT_NODE){
    for (size_t i = parent_node ; i < NODE_TABLE_COUNT ; i++) {
        if (tree->nodes[i].parent == parent_node && tree->nodes[i].weight != 0 ){
          brother_node = i ;
          break;
        }
      }
      tree->nodes[brother_node].parent = tree->nodes[parent_node].parent ;
      if (!tree->nodes[brother_node].child_is_leaf){
          tree->nodes[tree->nodes[brother_node].child].parent = parent_node ;
          tree->nodes[tree->nodes[brother_node].child + 1].parent = parent_node ;
      }
      else {
        tree->leaf[tree->nodes[brother_node].child] = parent_node;
      }
      tree->nodes[parent_node] = tree->nodes[brother_node];
      clean_node(tree , brother_node);
      clean_node(tree ,tree->leaf[c] );
      tree->leaf[c] = -1 ;
 }
}

void clean_node(tree , node)
  TREE * tree ;
  int node ;
{
      tree->nodes[node].parent = 0;
      tree->nodes[node].child = 0;
      tree->nodes[node].weight = 0;
}


#ifndef ALPHANUMERIC

#define LEFT_END  218
#define RIGHT_END 191
#define CENTER    193
#define LINE      196
#define VERTICAL  179

#else

#define LEFT_END  '+'
#define RIGHT_END '+'
#define CENTER    '+'
#define LINE      '-'
#define VERTICAL  '|'

#endif

struct row {
    int first_member;
    int count;
} rows[ 32 ];

struct location {
    int row;
    int next_member;
    int column;
} positions[ NODE_TABLE_COUNT ];


void print_connecting_lines( tree, row )
TREE *tree;
int row;
{
    int current_col;
    int start_col;
    int end_col;
    int center_col;
    int node;
    int parent;

    current_col = 0;
    node = rows[ row ].first_member;
    while ( node != -1 ) {
        start_col = positions[ node ].column + 2;
        node = positions[ node ].next_member;
        end_col = positions[ node ].column + 2;
        parent = tree->nodes[ node ].parent;
        center_col = positions[ parent ].column;
        center_col += 2;
        for ( ; current_col < start_col ; current_col++ )
            putc( ' ', stdout );
        putc( LEFT_END, stdout );
        for ( current_col++ ; current_col < center_col ; current_col++ )
            putc( LINE, stdout );
        putc( CENTER, stdout );
        for ( current_col++; current_col < end_col ; current_col++ )
            putc( LINE, stdout );
        putc( RIGHT_END, stdout );
        current_col++;
        node = positions[ node ].next_member;
    }
    printf( "\n" );
}

/*
 * Printing the node numbers is pretty easy.
 */

void print_node_numbers( row )
int row;
{
    int current_col;
    int node;
    int print_col;

    current_col = 0;
    node = rows[ row ].first_member;
    while ( node != -1 ) {
        print_col = positions[ node ].column + 1;
        for ( ; current_col < print_col ; current_col++ )
            putc( ' ', stdout );
        printf( "%03d", node );
        current_col += 3;
        node = positions[ node ].next_member;
    }
    printf( "\n" );
}

/*
 * Printing the weight of each node is easy too.
 */

void print_weights( tree, row )
TREE *tree;
int row;
{
    int current_col;
    int print_col;
    int node;
    int print_size;
    int next_col;
    char buffer[ 10 ];

    current_col = 0;
    node = rows[ row ].first_member;
    while ( node != -1 ) {
        print_col = positions[ node ].column + 1;
        sprintf( buffer, "%lu", tree->nodes[ node ].weight );
        if ( strlen( buffer ) < 3 )
            sprintf( buffer, "%03lu", tree->nodes[ node ].weight );
        print_size = 3;
        if ( strlen( buffer ) > 3 ) {
            if ( positions[ node ].next_member == -1 )
                print_size = strlen( buffer );
            else {
                next_col = positions[ positions[ node ].next_member ].column;
                if ( ( next_col - print_col ) > 6 )
                    print_size = strlen( buffer );
                else {
                    strcpy( buffer, "---" );
                    print_size = 3;
                }
            }
        }
        for ( ; current_col < print_col ; current_col++ )
            putc( ' ', stdout );
        printf(" %s ", buffer );
        current_col += print_size;
        node = positions[ node ].next_member;
    }
    printf( "\n" );
}

/*
 * Printing the symbol values is a little more complicated.  If it is a
 * printable symbol, I print it between simple quote characters.  If
 * it isn't printable, I print a hex value, which also only takes up three
 * characters.  If it is an internal node, it doesn't have a symbol,
 * which means I just print the vertical line.  There is one complication
 * in this routine.  In order to save space, I check first to see if
 * any of the nodes in this row have a symbol.  If none of them have
 * symbols, we just skip this part, since we don't have to print the
 * row at all.
 */

void print_symbol( tree, row )
TREE *tree;
int row;
{
    int current_col;
    int print_col;
    int node;

    current_col = 0;
    node = rows[ row ].first_member;
    while ( node != -1 ) {
        if ( tree->nodes[ node ].child_is_leaf )
            break;
        node = positions[ node ].next_member;
    }
    if ( node == -1 )
        return;
    node = rows[ row ].first_member;
    while ( node != -1 ) {
        print_col = positions[ node ].column + 1;
        for ( ; current_col < print_col ; current_col++ )
            putc( ' ', stdout );
        if ( tree->nodes[ node ].child_is_leaf ) {
            if ( isprint( tree->nodes[ node ].child ) )
                printf( "'%c'", tree->nodes[ node ].child );
            else if ( tree->nodes[ node ].child == END_OF_STREAM )
                printf( "EOF" );
            else if ( tree->nodes[ node ].child == ESCAPE )
                printf( "ESC" );
            else
                printf( "%02XH", tree->nodes[ node ].child );
        } else
            printf( " %c ", VERTICAL );
        current_col += 3;
        node = positions[ node ].next_member;
    }
    printf( "\n" );
}

/************************** End of AHUFF.C ****************************/
