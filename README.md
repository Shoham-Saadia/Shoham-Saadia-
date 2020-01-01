# Shoham-Saadia-
Forward looking (dynamic Huffman coding)

It is my thesis algorithm that is based on dynamic Hoffman coding but instead based on the Huffman tree on the characters already read it bases the Tree on the characters that are expected to be read.

The code is written in C language and is based on the code from https://github.com/studiawan/data-compression/tree/master/adaptive_huffman that base on Data Compression Book by M. Nelson and J.L. Gailly.

To compress use main-c in the build operation and then:
./run_Forward_c.o <source> <destination>
  while source is the text file and destination is the compress binary file.
  
To decode, use main-e in the build operation and then:
./run_Forward_e.o <source> <destination>
    while source is the compress binary file of Forward_c and the destination is the decode file.
