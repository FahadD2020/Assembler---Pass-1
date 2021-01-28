#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define DEBUG 0
#define MAX_ADDR 32768
#define MAX_ADDR_STR "32768"
#define MAX_LABEL_LEN 6
#define HT_CAPACITY 100
#define WORD_LEN_BYTES 3    // SIC word = 3 bytes
#define MAX_WORD_VAL 0x00ffffff

void pass_one(FILE *);
void replace_char(char *line, char what, char to);
void get_tokens(char *line, char **label, char **opcode, 
  char **operand, int line_num);
void error_exit(char *line, int line_num, char *msg);
bool is_digits(char *str);
bool valid_label(char *label);
bool valid_address(char *addr);
bool is_blank_line(char *line);
int count_char(char *str, char ch);
int get_byte_const_len(char *str, char *line);
int find_byte_str_len(char *line);
int get_hex_str_byte_len(char *str, int len);
void init_optab();
void init_directives();
void init_symtab();


/** things for hash table **/
/** ht collision resolution using chaining **/
struct ht_node {
  char *key;
  int value;
  struct ht_node *next;
};

typedef struct ht_node ht_node;

// 3 hash tables to use here
ht_node *SYMTAB[HT_CAPACITY];
ht_node *OPTAB[HT_CAPACITY];
ht_node *DIRECTIVES[HT_CAPACITY];

int insert(ht_node *ht[], char *key, int value);
bool search(ht_node *ht[], char *key);
unsigned long hash(char *key);
void print_hashtable(ht_node *ht[]);


int main(int argc, char *argv[]) {

	FILE *fp;
  if (argc != 2) {
  	printf("Incorrect usage! Correct usage is:\n");
  	printf("./project1 <SIC-assembly-file>\n");
  	return 1;
  }

  fp = fopen(argv[1], "r");
  if (fp == NULL) {
  	printf("Error in opening the file!\n");
  	return 2;
  }

  init_optab();
  init_directives();
  init_symtab();

  pass_one(fp);             // function to perform pass1 of assembly
  print_hashtable(SYMTAB);  // print out symbol table

  fclose(fp);
  return 0;
}

/*
directives is a hash table with values as 1 (value isn't used). 
Initialize it.
*/
void init_directives() {
  for (int i = 0; i < HT_CAPACITY; i++) 
    DIRECTIVES[i] = NULL;

  insert(DIRECTIVES, "START",  1);
  insert(DIRECTIVES, "END",  1);
  insert(DIRECTIVES, "BYTE", 1);
  insert(DIRECTIVES, "WORD", 1);
  insert(DIRECTIVES, "RESB", 1);
  insert(DIRECTIVES, "RESW", 1);
  insert(DIRECTIVES, "RESR", 1);
  insert(DIRECTIVES, "EXPORTS", 1);
}

/*
hash table of opcode to value
TODO: probably better to read from a file as this is a static table.
*/
void init_optab() {
  for (int i = 0; i < HT_CAPACITY; i++) 
    OPTAB[i] = NULL;

  insert(OPTAB, "ADD", 0x18);
  insert(OPTAB, "AND", 0x40);
  insert(OPTAB, "COMP", 0x28);
  insert(OPTAB, "DIV", 0x24);
  insert(OPTAB, "FIX", 0xC4);
  insert(OPTAB, "J", 0x3C);
  insert(OPTAB, "JEQ", 0x30);
  insert(OPTAB, "JGT", 0x34);
  insert(OPTAB, "JLT", 0x38);
  insert(OPTAB, "JSUB", 0x48);
  insert(OPTAB, "LDA", 0x00);
  insert(OPTAB, "LDB", 0x68);
  insert(OPTAB, "LDCH", 0x50);
  insert(OPTAB, "LDL", 0x08);
  insert(OPTAB, "LDX", 0x04);
  insert(OPTAB, "MUL", 0xD0);
  insert(OPTAB, "OR", 0x44);
  insert(OPTAB, "RD", 0xD8);
  insert(OPTAB, "RSUB", 0x4C);
  insert(OPTAB, "STA", 0x0C);
  insert(OPTAB, "STB", 0x78);
  insert(OPTAB, "STCH", 0x54);
  insert(OPTAB, "STL", 0x14);
  insert(OPTAB, "STX", 0x10);
  insert(OPTAB, "SUB", 0x1C);
  insert(OPTAB, "TD", 0xE0);
  insert(OPTAB, "TIX", 0x2C);
  insert(OPTAB, "WD", 0xDC);
}

/*
hash table for symbol table.
*/
void init_symtab() {
  for (int i = 0; i < HT_CAPACITY; i++) 
    SYMTAB[i] = NULL;
}

/*
Perform pass 1 of the SIC assembly and create the SYMTAB. 
*/
void pass_one(FILE *fp) {
  char *line = NULL;
  size_t len = 0;
  char *label, *opcode, *operand;
  int line_num = 0;
  int loc_ctr;
  int byte_const_len = -1;
  bool end_found = false;

  // loop to skip all comments
  while (getline(&line, &len, fp) != -1) {
    line_num++;

    if (is_blank_line(line)) {
      error_exit(line, line_num, "Blank lines are not allowed");
    }

    if (*line != '#') {
      // breaking on first non comment line
      break;
    }
  }

  // now line is the 1st non comment line
  if (line == NULL || strlen(line) == 0) {
    printf("ERROR: source file has no code!\n");
    exit(3);
  }

  // process the 1st line
  replace_char(line, '\t', ' ');     // replace all tabs with a single space
  get_tokens(line, &label, &opcode, &operand, line_num);

  if (strcmp("START", opcode) != 0) {
    error_exit(line, line_num, "File must start with \"START\" opcode");
  }

  if (!valid_address(operand)) {
    error_exit(line, line_num, "START operand must be a valid address");
  }

  // loc_ctr is now the start address (its a hex string. Converting to int)
  loc_ctr = (int) strtol(operand, NULL, 16);

  if (loc_ctr > MAX_ADDR) {
    error_exit(line, line_num, "START address must be < "MAX_ADDR_STR);
  }

  if (label != NULL && valid_label(label)) {
    insert(SYMTAB, label, loc_ctr);
  } else {
    error_exit(line, line_num, "label is invalid");
  }

  if (DEBUG) {
    printf("current line: %s\n", line);
    printf("label: %s|opcode: %s|operand: %s\n", label, opcode, operand);
    printf("*************\n");
    print_hashtable(SYMTAB);
    printf("*************\n");
    printf("label: %s|opcode: %s|operand: %s\n\n", label, opcode, operand);
  }

  // TODO for pass 2: write line to intermediate file as per pass1 algorithm

  // processing the remaining lines
  while ((getline(&line, &len, fp)) != -1) {
    line_num++;
    if (is_blank_line(line)) {
      error_exit(line, line_num, "Blank lines are not allowed");
    }

    replace_char(line, '\t', ' ');     // replace all tabs with a single space
    get_tokens(line, &label, &opcode, &operand, line_num);

    // there should not be any more instructions after END
    // This should never be printed as we break on finding END
    if (end_found && opcode != NULL) {
      error_exit(line, line_num, "Can't have more instructions after END");
    }

    if (strcmp("END", opcode) == 0) {
      if (end_found) {
        error_exit(line, line_num, "Duplicate END opcode");
      } else {
        end_found = true;
        break;  // don't read more lines
      }
    }

    if (*line == '#') {
      // this is a comment line (starts with "#")
      // TODO write to intermediate file
      continue;
    }

    if (DEBUG) {
      printf("current line: %s\n", line);
      printf("label: %s|opcode: %s|operand: %s\n", label, opcode, operand);
      printf("*************\n");
      print_hashtable(SYMTAB);
      printf("*************\n");
      printf("label: %s|opcode: %s|operand: %s\n\n", label, opcode, operand);
    }

    // now the line is not a comment. Processing further.
    if (label != NULL) {
      if (search(SYMTAB, label)) {
        error_exit(line, line_num, "Duplicate label in instruction");
      } else if (valid_label(label) == false) {
        error_exit(line, line_num, "label is invalid");
      } else {
        insert(SYMTAB, label, loc_ctr);
      }
    }

    bool opcode_found = search(OPTAB, opcode);
    if (opcode_found) {
      loc_ctr += WORD_LEN_BYTES;
    } else if (strcmp("WORD", opcode) == 0) {
      if (!is_digits(operand)) {
        error_exit(line, line_num, "WORD value must be digits");
      } else if ((unsigned) atoi(operand) > MAX_WORD_VAL) {
        error_exit(line, line_num, "WORD value must be within 24 bits");
      } else {
        loc_ctr += WORD_LEN_BYTES;
      }
    } else if (strcmp("RESW", opcode) == 0) {

      if (!is_digits(operand)) {
        error_exit(line, line_num, "RESW value must be digits");
      } else {
        loc_ctr += WORD_LEN_BYTES*atoi(operand);
      }

    } else if (strcmp("RESB", opcode) == 0) {

      if (!is_digits(operand)) {
        error_exit(line, line_num, "RESB value must be digits");
      } else {
        loc_ctr += atoi(operand);
      }
    } else if (strcmp("BYTE", opcode) == 0) {
      byte_const_len = get_byte_const_len(operand, line);
      if (byte_const_len < 0) {
        error_exit(line, line_num, "Invalid BYTE constant");
      } 
      loc_ctr += byte_const_len;
    } else {
      error_exit(line, line_num, "Invalid operation code");
    }

    if (loc_ctr > MAX_ADDR) {
      error_exit(line, line_num, "Program must fit within "MAX_ADDR_STR);
    }
  }

  if (!end_found) {
    printf("ERROR: source file has no END instruction\n");
    exit(3);
  }

  // if we reached here, pass1 is good
}

/*
return true if the line is blank. false otherwise
*/
bool is_blank_line(char *line) {
  bool is_blank = true;
  int i;
  char c;

  if (line != NULL) {
    for (i = 0; i < strlen(line); i++) {
      c = line[i];
      if (c != '\t' && c != ' ' && c != '\n' && c != '\r') {
        is_blank = false;
        break;
      }
    }
  }

  return is_blank;
}


/*
Return true if a given label is valid as per specs.
*/
bool valid_label(char *label) {
  if (label == NULL || strlen(label) > MAX_LABEL_LEN) {
    //printf("label is NULL or too long\n");
    return false;
  }

  if (*label < 'A' || *label > 'Z') {
    //printf("label must start with an alpha character [A-Z]\n");
    return false;
  }

  if (search(DIRECTIVES, label)) {
    //printf("label can't be a directive name\n");
    return false;
  }

  for (int i = 0; i < strlen(label); i++) {
    if (label[i] == ' ' || label[i] == '$' || label[i] == '!' 
      || label[i] == '=' || label[i] == '+' || label[i] == '-' 
      || label[i] == '(' || label[i] == ')' || label[i] == '@') {
      //printf("label contains non-allowed chars\n");
      return false;
    }
  }
  return true;
}

/*
Given a byte constant string, find and return it's length. Return -1 if
there is any error. Some examples:
X'F1'   -> 1     // 1 byte (X) 0xF1
X'F1F2' -> 2     // 2 byte (X) 0xF1F2 (note we will consider only upper case)
X'F1F'  -> -1    // 1.5 byte is invalid
XF1     -> -1    // quotes are missing
X'F1    -> -1    // quote is missing
X'FG'   -> -1    // invalid hex byte 0xFG
C'ABC'  -> 3     // 3 chars (C) 'ABC'

So..
0th char is 'X' or 'C'
1st char is single quote
last char is single quote
No single quote in between
*/
int get_byte_const_len(char *str, char *line) {
  int len = strlen(str);

  if (str[0] != 'X' && str[0] != 'C') {
    // must start with either X or C
    return -1;
  }

  if (str[0] == 'C') {
    // need to find length from C'.....' in line
    return find_byte_str_len(line);
  }

  if (str[1] != '\'' || str[len-1] != '\'') {
    // must be of the form X'...'
    return -1;
  }

  // for hex, this should be a string with hex digits and of even length
  // "X'ABCD'" -> "ABCD'", 4
  return get_hex_str_byte_len(str+2, len-3);
}

/*
This finds the byte constant str length for byte string.
SIZE BYTE C'string has space'   -> 16


Since tokens are broken on spaces, operand isn't read correctly in case of
BYTE operand containing spaces. Need to scan again.
Return -1 incase of errors
*/
int find_byte_str_len(char *line) {
  char *c;
  int len;

  if (line == NULL || count_char(line, '\'') != 2) {
    // there should be only 2 single quotes - start and end
    return -1;
  }

  c = line;
  while (*c != '\0') {
    if (*c == 'C' && *(c+1) == '\'') {
      // reached C'
      break;
    }
    c++;
  }

  c += 2;
  len = 0;
  while (*c != '\'') {
    // loop till ending ' is not found
    len++;
    c++;
  }

  return len;
}

/*
Return the no. of bytes in a hex string str. If str contains non-hex chars 
then or len is odd then return -1 (error). Lowercase hex chars are also 
invalid. For e.g.
"FFF'", 3  -> -1  (even length 3)
"A9C0K", 4 ->  2  (length is even and 1st 4 chars are hex. This is 2 bytes)
"ab01", 4  -> -1  (lowercase hex are invalid for now)
*/
int get_hex_str_byte_len(char *str, int len) {
  int i;

  if ((len % 2) == 1) {
    // odd length ain't no good
    return -1;
  }

  i = 0;
  while (i < len) {
    if (str[i] < '0') {
      return -1;
    } else if (str[i] > '9' && str[i] < 'A') {
      return -1;
    } else if (str[i] > 'F') {
      return -1;
    }
    i++;
  }

  // valid hexstr. has len/2 bytes (len will be even here)
  return len/2;
}

/*
Count and return the number of occurrences of ch in str
*/
int count_char(char *str, char ch) {
  int count = 0;
  int i = 0;
  while (i < strlen(str)) {
    if (str[i] == ch) {
      count++;
    }
    i++;
  }
  return count;
}


/*
Replace all occurrence of 'what' in line with 'to'.
If the line has \n or \r character, it is replaced with \0 to mark
the end of line
*/
void replace_char(char *str, char what, char to) {
  char *ch = str;
  while (*ch != '\0') {
    if (*ch == '\r' || *ch == '\n') {
      *ch = '\0';
      break;
    } else if (*ch == what) {
      *ch = to;
    }
    ch++;
  }
}

/*
check if a string contains only digits (i.e, a decimal address or number)
*/
bool is_digits(char *str) {
  if (str == NULL) {
    return false;
  }

  int i = 0;
  while (i < strlen(str)) {
    if (!isdigit(str[i])) {
      return false;
    }
    i++;
  }
  return true;
}

/*
check if the string is a valid address (i.e, contains only [0-9][A-F])
*/
bool valid_address(char *str) {
  if (str == NULL) {
    return false;
  }

  int i = 0;
  while (i < strlen(str)) {
    if (str[i] < '0') {
      return false;
    } else if (str[i] > '9' && str[i] < 'A') {
      return false;
    } else if (str[i] > 'F') {
      return false;
    }
    i++;
  }
  return true;
}

/*
Given an assembly code line, get the label, opcode and operand.
Opcode maybe null.
*/
void get_tokens(char *line, char **label, 
  char **opcode, char **operand, int line_num) {
  char *cpy = (char *) malloc( (strlen(line) + 1) * sizeof(char) );
  char *tok;
  int i;
  int max_tok = 3;      // get maximum 3 tokens [label] opcode operand
  int max_tok_len = 20; // each token is upto max_tok_len chars
  char tokens[max_tok][max_tok_len];

  strcpy(cpy, line);
  i = 0;
  tok = strtok(cpy, " ");
  strcpy(tokens[i++], tok);

  while (tok != NULL && i < max_tok) {
    tok = strtok(NULL, " ");
    if (!tok) {
      break;
    }
    strcpy(tokens[i++], tok);
  }

  if (i == 1) {
    // only opcode
    *label = NULL;

    *opcode = (char *)malloc(sizeof(char) * (strlen(tokens[0]) + 1));
    strcpy(*opcode, tokens[0]);

    *operand = NULL;
  } else if (i == 2) {

    // only opcode and operand
    *label = NULL;

    *opcode = (char *)malloc(sizeof(char) * (strlen(tokens[0]) + 1));
    strcpy(*opcode, tokens[0]);

    *operand = (char *)malloc(sizeof(char) * (strlen(tokens[1]) + 1));
    strcpy(*operand, tokens[1]);

  } else if (i == 3) {
    // line has label, opcode and operand
    *label = (char *)malloc(sizeof(char) * (strlen(tokens[0]) + 1));
    strcpy(*label, tokens[0]);

    *opcode = (char *)malloc(sizeof(char) * (strlen(tokens[1]) + 1));
    strcpy(*opcode, tokens[1]);

    *operand = (char *)malloc(sizeof(char) * (strlen(tokens[2]) + 1));
    strcpy(*operand, tokens[2]);

  } else {
    error_exit(line, line_num, "Some error in parsing line");
  }
}

/*
print assembly error message and exit
*/
void error_exit(char *line, int line_num, char *msg) {
  printf("ASSEMBLY ERROR:\n");
  printf("%s\n", line);
  printf("Line %d: %s\n", line_num, msg);
  exit(3);
}


/*** following is hash table related code ***/

/* insert a given key into the hash table. Collision resolution by 
chaining.
It returns the index of the chain where this got inserted
*/
int insert(ht_node *ht[], char *key, int value) {
  ht_node *x;
  unsigned long i;

  x = (ht_node *) malloc(sizeof(ht_node));
  i = hash(key) % HT_CAPACITY;

  x->key = (char *) malloc((strlen(key) + 1)*sizeof(char));
  strcpy(x->key, key);

  x->value = value;
  x->next = ht[i];
  ht[i] = x;     // inserting x at the beginning of list in ht[i]

  return i;
}

/*
Search for a given key in the hash table. Return true or false.
*/
bool search(ht_node *ht[], char *key) {
  int i = hash(key) % HT_CAPACITY;
  ht_node *t = ht[i];

  while (t) {
    if (strcmp(t->key, key) == 0) {
      return true;
    }
    t = t->next;
  }

  return false;
}

/* compute and return the hash of a string 

*/
unsigned long hash(char *str) {
  unsigned long hash = 5381;
  int i = 0;
  while ( i<strlen(str) )
  {
    char c = str[i];
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    i++;
  }
  return hash;
}

/*
Print the contents of the hash table
*/
void print_hashtable(ht_node *ht[]) {
  for (int i = 0; i < HT_CAPACITY; i++) {
    ht_node *t = ht[i];
    while (t) {
      printf("%s\t%X\n", t->key, t->value);
      t = t->next;
    }
  }
}