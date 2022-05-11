//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Muhammad Z Khan";
const char *studentID   = "A15876135";
const char *email       = "mzkhan@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

//define number of bits required for indexing the BHT here. 
int ghistoryBits = 14; // Number of bits used for Global History
int lhistoryBits = 10;
int pcBits = 10;
int tagBits = 5;
int bpType;       // Branch Prediction Type
int verbose;
int MAX_WEIGHT_PERCEPTRON = 15;  //7 to -8 -> 4 bits
int MIN_WEIGHT_PERCEPTRON = -16;
int Y_THRESHOLD_PERCEPTRON = 4;



//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
//gshare
uint8_t *bht_gshare;
uint64_t ghistory;

//tournament
uint8_t *gbht_tournament;
uint8_t *lbht_tournament;
uint8_t *chooser_tournament;
uint64_t *lhistory_tournament;
uint64_t ghistory;

//custom (YAGS)
uint8_t *choice_yags;
uint32_t *taken_cache_yags;
uint32_t *nottaken_cache_yags;
uint64_t ghistory;
uint64_t taken_size, nottaken_size;
uint64_t cache_capacity;
uint64_t taken_start, taken_end;
uint64_t nottaken_start, nottaken_end;

//perceptron
int **perceptron_table;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

//perceptron functions
void init_perceptron() {
  ghistoryBits = 15;
  pcBits = 8;

  int nrows = (1 << pcBits);
  int ncols = (ghistoryBits+1);

  perceptron_table = malloc(nrows * sizeof(int *));
  perceptron_table[0] = malloc(nrows * ncols * sizeof(int));
  int i = 0;
  for (i = 1; i < nrows; ++i) {
    perceptron_table[i] = perceptron_table[0] + (i * ncols);
  }

  int j = 0;
  for (i = 0; i < nrows; ++i) {
    for (j = 0; j < ncols; ++j) {
      perceptron_table[i][j] = 0;
    }
  }

  ghistory = 0;
}

uint8_t 
perceptron_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;

  uint32_t pc_lower_bits = pc & ((1 << pcBits) - 1);
  uint32_t ghistory_lower_bits = ghistory & ((1 << ghistoryBits) - 1);
  uint32_t i = pc_lower_bits;

  int x;
  int w;

  int y = perceptron_table[i][0];

  int j = 0;
  for (j = 1; j <= ghistoryBits; ++j) {
    w = perceptron_table[i][j];
    //if (w == 0) w = -1;

    x = (ghistory >> (ghistoryBits-j)) & 1;
    if (x == 0) x = -1;

    y += (w*x);
  }

  if (y < 0) {
    return NOTTAKEN;
  } else {
    return TAKEN;
  }
}


void
train_perceptron(uint32_t pc, uint8_t outcome) {
  int t = (outcome==TAKEN)?1:-1;

  uint32_t bht_entries = 1 << ghistoryBits;

  uint32_t pc_lower_bits = pc & ((1 << pcBits) - 1);
  uint32_t i = pc_lower_bits;

  int x;
  int w;

  int y = perceptron_table[i][0];

  int j = 0;
  for (j = 1; j <= ghistoryBits; ++j) {
    w = perceptron_table[i][j];
    //if (w == 0) w = -1;

    x = (ghistory >> (ghistoryBits-j)) & 1;
    if (x == 0) x = -1;

    y += (w*x);
  }

  int sign_y = 0;
  if (y < 0) {
    sign_y = -1;
  } else {
    sign_y = 1;
  }

  if ((sign_y != t) || (y >= -Y_THRESHOLD_PERCEPTRON && y <= Y_THRESHOLD_PERCEPTRON)) {
    for (j = 0; j <= ghistoryBits; ++j) {
      w = perceptron_table[i][j];
      //if (w == 0) w = -1;

      if (j == 0) {
        x = 1;
      } else {
        x = (ghistory >> (ghistoryBits-j)) & 1;
        if (x == 0) x = -1;
      }

      int xt = x*t;

      if (((xt > 0) && (w < MAX_WEIGHT_PERCEPTRON)) || ((xt < 0) && (w > MIN_WEIGHT_PERCEPTRON))) {
        perceptron_table[i][j] = w + xt;
      }

    }
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}

//yags funtions
void init_yags() {
  ghistoryBits = 9;
  pcBits = 11;
  tagBits = 11;

  int choice_entries = 1 << pcBits;
  choice_yags = (uint8_t*)malloc(choice_entries * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< choice_entries; i++){
    choice_yags[i] = WN;
  }

  int block_size = tagBits + 2;
  cache_capacity = 1 << (ghistoryBits-1); //subtract 1 cuz 2-way set
  taken_cache_yags = (uint32_t*)malloc(cache_capacity * sizeof(uint32_t));
  nottaken_cache_yags = (uint32_t*)malloc(cache_capacity * sizeof(uint32_t));
  for(i = 0; i< cache_capacity; i++){
    taken_cache_yags[i] = (WN << block_size) | WN;
    nottaken_cache_yags[i] = (WT << block_size) | WT;
  }

  taken_size = 0;
  nottaken_size = 0;
  taken_start = 0;
  taken_end = 0;
  nottaken_start = 0;
  nottaken_end = 0;

  ghistory = 0;
}

uint8_t 
yags_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t choice_entries = 1 << pcBits;
  uint32_t pc_lower_bits = pc & (choice_entries-1);
  uint32_t choice_index = pc_lower_bits;
  uint8_t choice_predict = choice_yags[choice_index];

  pc_lower_bits = pc & (cache_capacity-1);
  uint32_t ghistory_lower_bits = ghistory & (cache_capacity-1);
  uint32_t cache_index = pc_lower_bits ^ ghistory_lower_bits;

  uint32_t tag_entries = 1 << tagBits;
  pc_lower_bits = pc & (tag_entries-1);

  uint32_t two_bc = choice_predict;

  if ((choice_predict == WN || choice_predict == SN)/* && (cache_index < taken_size)*/) {
    uint32_t cache_entry = taken_cache_yags[cache_index];
    // uint32_t lru_bit_mask = 1 << tagBits;
    uint32_t tag0 = cache_entry >> (2+tagBits+2);
    tag0 = tag0 & (tag_entries-1);
    if (tag0 == pc_lower_bits) {
      two_bc = (cache_entry >> (tagBits+2)) & 3;
    } 
    uint32_t tag1 = cache_entry >> 2;
    tag1 = tag1 & (tag_entries-1);
    if (tag1 == pc_lower_bits) {
      two_bc = cache_entry & 3;
    }
  } else if ((choice_predict == WT || choice_predict == ST)/* && (cache_index < nottaken_size)*/) {
    uint16_t cache_entry = nottaken_cache_yags[cache_index];

    uint32_t tag0 = cache_entry >> (2+tagBits+2);
    tag0 = tag0 & (tag_entries-1);
    if (tag0 == pc_lower_bits) {
      two_bc = (cache_entry >> (tagBits+2)) & 3;
    } 
    uint32_t tag1 = cache_entry >> 2;
    tag1 = tag1 & (tag_entries-1);
    if (tag1 == pc_lower_bits) {
      two_bc = cache_entry & 3;
    }
  } else {
    printf("Warning: Undefined state of entry in YAGS choice BHT!\n");
    return NOTTAKEN;
  }

  switch(two_bc){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in YAGS cache!\n");
      return NOTTAKEN;
  }

}

void
train_yags(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t choice_entries = 1 << pcBits;
  uint32_t pc_lower_bits = pc & (choice_entries-1);
  uint32_t choice_index = pc_lower_bits;
  uint8_t choice_predict = choice_yags[choice_index];

  // uint8_t choice_correct = 0;
  uint8_t cache_correct = 0;

  // if (outcome==TAKEN && (choice_predict==WT || choice_predict==ST)) {
  //   choice_correct = 1;
  //   choice_yags[choice_index] = ST;
  // }

  // if (outcome==NOTTAKEN && (choice_predict==WN || choice_predict==SN)) {
  //   choice_correct = 1;
  //   choice_yags[choice_index] = SN;
  // }

  pc_lower_bits = pc & (cache_capacity-1);
  uint32_t ghistory_lower_bits = ghistory & (cache_capacity-1);
  uint32_t cache_index = pc_lower_bits ^ ghistory_lower_bits;

  uint32_t tag_entries = 1 << tagBits;
  pc_lower_bits = pc & (tag_entries-1);

  uint32_t two_bc;

  if (choice_predict==WN || choice_predict==SN) {
    uint16_t cache_entry = taken_cache_yags[cache_index];
    
    uint32_t tag0 = cache_entry >> (2+tagBits+2);
    tag0 = tag0 & (tag_entries-1);
    
    uint32_t tag1 = cache_entry >> 2;
    tag1 = tag1 & (tag_entries-1);

    if (tag0 == pc_lower_bits) {
      two_bc = (cache_entry >> (tagBits+2)) & 3;
      switch(two_bc){
        case WN:
          two_bc = (outcome==TAKEN)?WT:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case SN:
          two_bc = (outcome==TAKEN)?WN:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case WT:
          two_bc = (outcome==TAKEN)?ST:WN;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        case ST:
          two_bc = (outcome==TAKEN)?ST:WT;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        default:
          printf("Warning: Undefined state of entry in YAGS Cache!\n");
      }
      uint32_t mask = ~(3 << (tagBits+2));
      taken_cache_yags[cache_index] = (taken_cache_yags[cache_index] & mask) | (two_bc << (tagBits+2));
      uint32_t lru_bit = 0 << (tagBits+2+tagBits+2);
      taken_cache_yags[cache_index] = taken_cache_yags[cache_index] | lru_bit;
      // printf("taken[%d]: %d used \n", cache_index, tag);
    } else if (tag1 == pc_lower_bits) {
      two_bc = cache_entry & 3;
      switch(two_bc){
        case WN:
          two_bc = (outcome==TAKEN)?WT:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case SN:
          two_bc = (outcome==TAKEN)?WN:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case WT:
          two_bc = (outcome==TAKEN)?ST:WN;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        case ST:
          two_bc = (outcome==TAKEN)?ST:WT;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        default:
          printf("Warning: Undefined state of entry in YAGS Cache!\n");
      }
      uint32_t mask = ~3;
      taken_cache_yags[cache_index] = (taken_cache_yags[cache_index] & mask) | two_bc;
      uint32_t lru_bit = 1 << (tagBits+2+tagBits+2);
      taken_cache_yags[cache_index] = taken_cache_yags[cache_index] | lru_bit; 
    } else if (outcome!=NOTTAKEN) {
      uint32_t lru_bit = (cache_entry >> (tagBits+2+tagBits+2)) & 1;
      uint32_t two_bc_0 = (cache_entry >> (tagBits+2)) & 3;
      uint32_t two_bc_1 = cache_entry & 3;

      if (((two_bc_0 == WN || two_bc_0 == SN) && (two_bc_1 == WT || two_bc_1 == ST)) || ((lru_bit == 1) && 
      (((two_bc_0 == WT || two_bc_0 == ST) && (two_bc_1 == WT || two_bc_1 == ST)) || 
      ((two_bc_0 == WN || two_bc_0 == SN) && (two_bc_1 == WN || two_bc_1 == SN))))) {
        uint32_t mask = ~((tag_entries-1) << (2+tagBits+2)); //tag0 mask
        tag0 = pc_lower_bits << (2+tagBits+2);
        taken_cache_yags[cache_index] = (taken_cache_yags[cache_index] & mask) | tag0;

        mask = ~(3 << (tagBits+2));
        two_bc = WT << (tagBits+2);
        taken_cache_yags[cache_index] = (taken_cache_yags[cache_index] & mask) | two_bc;

        uint32_t lru_bit = 0 << (tagBits+2+tagBits+2);
        taken_cache_yags[cache_index] = taken_cache_yags[cache_index] | lru_bit; 
      } else {
        uint32_t mask = ~((tag_entries-1) << 2); //tag0 mask
        tag1 = pc_lower_bits << 2;
        taken_cache_yags[cache_index] = (taken_cache_yags[cache_index] & mask) | tag1;

        mask = ~3;
        two_bc = WT;
        taken_cache_yags[cache_index] = (taken_cache_yags[cache_index] & mask) | two_bc;

        uint32_t lru_bit = 1 << (tagBits+2+tagBits+2);
        taken_cache_yags[cache_index] = taken_cache_yags[cache_index] | lru_bit;
      }
      // taken_cache_yags[cache_index] = (pc_lower_bits << 2) | WT;
      // printf("taken[%d]: (%d, %d) replaced by %d \n", cache_index, tag, cache_entry & 3, pc_lower_bits);
    }

    if (outcome==NOTTAKEN) {
      choice_yags[choice_index] = SN;
    } else if (cache_correct == 0) {
      if (choice_index == WN) {
        choice_yags[choice_index] = WT;
      } else {
        choice_yags[choice_index] = WN;
      }
    }

  } else if (choice_predict==WT || choice_predict==ST) {
    uint16_t cache_entry = nottaken_cache_yags[cache_index];
    
    uint32_t tag0 = cache_entry >> (2+tagBits+2);
    tag0 = tag0 & (tag_entries-1);
    
    uint32_t tag1 = cache_entry >> 2;
    tag1 = tag1 & (tag_entries-1);
    
    if (tag0 == pc_lower_bits) {
      two_bc = (cache_entry >> (tagBits+2)) & 3;
      switch(two_bc){
        case WN:
          two_bc = (outcome==TAKEN)?WT:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case SN:
          two_bc = (outcome==TAKEN)?WN:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case WT:
          two_bc = (outcome==TAKEN)?ST:WN;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        case ST:
          two_bc = (outcome==TAKEN)?ST:WT;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        default:
          printf("Warning: Undefined state of entry in YAGS Cache!\n");
      }
      uint32_t mask = ~(3 << (tagBits+2));
      nottaken_cache_yags[cache_index] = (nottaken_cache_yags[cache_index] & mask) | (two_bc << (tagBits+2));
      uint32_t lru_bit = 0 << (tagBits+2+tagBits+2);
      nottaken_cache_yags[cache_index] = nottaken_cache_yags[cache_index] | lru_bit;
      // printf("taken[%d]: %d used \n", cache_index, tag);
    } else if (tag1 == pc_lower_bits) {
      two_bc = cache_entry & 3;
      switch(two_bc){
        case WN:
          two_bc = (outcome==TAKEN)?WT:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case SN:
          two_bc = (outcome==TAKEN)?WN:SN;
          cache_correct = (outcome==TAKEN)?0:1;
          break;
        case WT:
          two_bc = (outcome==TAKEN)?ST:WN;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        case ST:
          two_bc = (outcome==TAKEN)?ST:WT;
          cache_correct = (outcome==TAKEN)?1:0;
          break;
        default:
          printf("Warning: Undefined state of entry in YAGS Cache!\n");
      }
      uint32_t mask = ~3;
      nottaken_cache_yags[cache_index] = (nottaken_cache_yags[cache_index] & mask) | two_bc;
      uint32_t lru_bit = 1 << (tagBits+2+tagBits+2);
      nottaken_cache_yags[cache_index] = nottaken_cache_yags[cache_index] | lru_bit; 
      // printf("nottaken[%d]: %d used \n", cache_index, tag);
    } else if (outcome!=TAKEN) {
      uint32_t lru_bit = (cache_entry >> (tagBits+2+tagBits+2)) & 1;
      uint32_t two_bc_0 = (cache_entry >> (tagBits+2)) & 3;
      uint32_t two_bc_1 = cache_entry & 3;

      if (((two_bc_0 == WT || two_bc_0 == ST) && (two_bc_1 == WN || two_bc_1 == SN)) || ((lru_bit == 1) && 
      (((two_bc_0 == WT || two_bc_0 == ST) && (two_bc_1 == WT || two_bc_1 == ST)) || 
      ((two_bc_0 == WN || two_bc_0 == SN) && (two_bc_1 == WN || two_bc_1 == SN))))) {
        uint32_t mask = ~((tag_entries-1) << (2+tagBits+2)); //tag0 mask
        tag0 = pc_lower_bits << (2+tagBits+2);
        nottaken_cache_yags[cache_index] = (nottaken_cache_yags[cache_index] & mask) | tag0;

        mask = ~(3 << (tagBits+2));
        two_bc = WT << (tagBits+2);
        nottaken_cache_yags[cache_index] = (nottaken_cache_yags[cache_index] & mask) | two_bc;

        uint32_t lru_bit = 0 << (tagBits+2+tagBits+2);
        nottaken_cache_yags[cache_index] = nottaken_cache_yags[cache_index] | lru_bit; 
      } else {
        uint32_t mask = ~((tag_entries-1) << 2); //tag0 mask
        tag1 = pc_lower_bits << 2;
        nottaken_cache_yags[cache_index] = (nottaken_cache_yags[cache_index] & mask) | tag1;

        mask = ~3;
        two_bc = WT;
        nottaken_cache_yags[cache_index] = (nottaken_cache_yags[cache_index] & mask) | two_bc;

        uint32_t lru_bit = 1 << (tagBits+2+tagBits+2);
        nottaken_cache_yags[cache_index] = nottaken_cache_yags[cache_index] | lru_bit;
      }
      // nottaken_cache_yags[cache_index] = (pc_lower_bits << 2) | WT;
      // printf("nottaken[%d]: (%d, %d) replaced by %d \n", cache_index, tag, cache_entry & 3, pc_lower_bits);
    }

    if (outcome==TAKEN) {
      choice_yags[choice_index] = ST;
    } else if (cache_correct == 0) {
      if (choice_index == WT) {
        choice_yags[choice_index] = WN;
      } else {
        choice_yags[choice_index] = WT;
      }
    }

  } else {
    printf("Warning: Undefined state of entry in YAGS Choice BHT!\n");
  }


  // if ((outcome==TAKEN && (choice_predict==WT || choice_predict==ST)) || 
  //     (outcome==NOTTAKEN && (choice_predict==WN || choice_predict==SN))) {
  //   choice_correct = 1;
  // }



  //Update state of entry in bht based on outcome
  // switch(bht_gshare[index]){
  //   case WN:
  //     bht_gshare[index] = (outcome==TAKEN)?WT:SN;
  //     break;
  //   case SN:
  //     bht_gshare[index] = (outcome==TAKEN)?WN:SN;
  //     break;
  //   case WT:
  //     bht_gshare[index] = (outcome==TAKEN)?ST:WN;
  //     break;
  //   case ST:
  //     bht_gshare[index] = (outcome==TAKEN)?ST:WT;
  //     break;
  //   default:
  //     printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  // }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}


//tournament funtions
void init_tournament() {
  ghistoryBits = 12;
  lhistoryBits = 11;
  pcBits = 10;

  int gbht_entries = 1 << ghistoryBits;
  int lbht_entries = 1 << lhistoryBits;
  int lhistory_entries = 1 << pcBits;

  gbht_tournament = (uint8_t*)malloc(gbht_entries * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< gbht_entries; i++){
    gbht_tournament[i] = WN;
  }

  lbht_tournament = (uint8_t*)malloc(lbht_entries * sizeof(uint8_t));
  // int i = 0;
  for(i = 0; i< lbht_entries; i++){
    lbht_tournament[i] = WN;
  }

  chooser_tournament = (uint8_t*)malloc(gbht_entries * sizeof(uint8_t));
  // int i = 0;
  for(i = 0; i< gbht_entries; i++){
    chooser_tournament[i] = WN;
  }

  lhistory_tournament = (uint64_t*)malloc(lhistory_entries * sizeof(uint64_t));
  // int i = 0;
  for(i = 0; i< lhistory_entries; i++){
    lhistory_tournament[i] = 0;
  }

  ghistory = 0;
}


uint8_t 
tournament_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t gbht_entries = 1 << ghistoryBits;
  uint32_t ghistory_lower_bits = ghistory & (gbht_entries -1);
  uint32_t gindex = ghistory_lower_bits;
  uint8_t gpredict = gbht_tournament[gindex];

  uint32_t lhistory_entries = 1 << pcBits;
  uint32_t pc_lower_bits = pc & (lhistory_entries-1);
  uint32_t pcindex = pc_lower_bits;
  uint64_t lhistory = lhistory_tournament[pcindex];

  uint32_t lbht_entries = 1 << lhistoryBits;
  uint32_t lhistory_lower_bits = lhistory & (lbht_entries -1);
  uint32_t lindex = lhistory_lower_bits;
  uint8_t lpredict = lbht_tournament[lindex];
  
  uint8_t chooser = chooser_tournament[gindex];

  switch(chooser){
    case WN:
      if (lpredict == WN || lpredict == SN) {
        return NOTTAKEN;  
      } else if (lpredict == WT || lpredict == ST) {
        return TAKEN;
      } else {
        printf("Warning: Undefined state of entry in Tournament local BHT!\n");
        return NOTTAKEN;
      }
    case SN:
      if (lpredict == WN || lpredict == SN) {
        return NOTTAKEN;  
      } else if (lpredict == WT || lpredict == ST) {
        return TAKEN;
      } else {
        printf("Warning: Undefined state of entry in Tournament local BHT!\n");
        return NOTTAKEN;
      }
    case WT:
      if (gpredict == WN || gpredict == SN) {
        return NOTTAKEN;  
      } else if (gpredict == WT || gpredict == ST) {
        return TAKEN;
      } else {
        printf("Warning: Undefined state of entry in Tournament global BHT!\n");
        return NOTTAKEN;
      }
    case ST:
      if (gpredict == WN || gpredict == SN) {
        return NOTTAKEN;  
      } else if (gpredict == WT || gpredict == ST) {
        return TAKEN;
      } else {
        printf("Warning: Undefined state of entry in Tournament global BHT!\n");
        return NOTTAKEN;
      }      
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
      return NOTTAKEN;
  }
}


void
train_tournament(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  // uint32_t bht_entries = 1 << ghistoryBits;
  // uint32_t pc_lower_bits = pc & (bht_entries-1);
  // uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  // uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  uint32_t gbht_entries = 1 << ghistoryBits;
  uint32_t ghistory_lower_bits = ghistory & (gbht_entries -1);
  uint32_t gindex = ghistory_lower_bits;
  uint8_t gpredict = gbht_tournament[gindex];

  uint32_t lhistory_entries = 1 << pcBits;
  uint32_t pc_lower_bits = pc & (lhistory_entries-1);
  uint32_t pcindex = pc_lower_bits;
  uint64_t lhistory = lhistory_tournament[pcindex];

  uint32_t lbht_entries = 1 << lhistoryBits;
  uint32_t lhistory_lower_bits = lhistory & (lbht_entries -1);
  uint32_t lindex = lhistory_lower_bits;
  uint8_t lpredict = lbht_tournament[lindex];

  uint8_t gcorrect = 0;
  uint8_t lcorrect = 0;

  if ((outcome==TAKEN && (gpredict==WT || gpredict==ST)) || 
      (outcome==NOTTAKEN && (gpredict==WN || gpredict==SN))) {
    gcorrect = 1;
  }

  if ((outcome==TAKEN && (lpredict==WT || lpredict==ST)) || 
      (outcome==NOTTAKEN && (lpredict==WN || lpredict==SN))) {
    lcorrect = 1;
  } 

  if (gcorrect != lcorrect) {

    // uint8_t chooser = chooser_tournament[gindex];  

    switch(chooser_tournament[gindex]){
      case WN:
        chooser_tournament[gindex] = (gcorrect==1)?WT:SN;
        break;
      case SN:
        chooser_tournament[gindex] = (gcorrect==1)?WN:SN;
        break;
      case WT:
        chooser_tournament[gindex] = (gcorrect==1)?ST:WN;
        break;
      case ST:
        chooser_tournament[gindex] = (gcorrect==1)?ST:WT;
        break;
      default:
        printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    }
  }

  //Update state of entry in bht based on outcome
  switch(lbht_tournament[lindex]){
    case WN:
      lbht_tournament[lindex] = (lcorrect==0)?WT:SN;
      break;
    case SN:
      lbht_tournament[lindex] = (lcorrect==0)?WN:SN;
      break;
    case WT:
      lbht_tournament[lindex] = (lcorrect==1)?ST:WN;
      break;
    case ST:
      lbht_tournament[lindex] = (lcorrect==1)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  lhistory_tournament[pcindex] = ((lhistory << 1) | outcome); 

  switch(gbht_tournament[gindex]){
    case WN:
      gbht_tournament[gindex] = (gcorrect==0)?WT:SN;
      break;
    case SN:
      gbht_tournament[gindex] = (gcorrect==0)?WN:SN;
      break;
    case WT:
      gbht_tournament[gindex] = (gcorrect==1)?ST:WN;
      break;
    case ST:
      gbht_tournament[lindex] = (gcorrect==1)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}



//gshare functions
void init_gshare() {
  // ghistoryBits = 14;
  int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< bht_entries; i++){
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}



uint8_t 
gshare_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(bht_gshare[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(bht_gshare[index]){
    case WN:
      bht_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}

void
cleanup_gshare() {
  free(bht_gshare);
}



void
init_predictor()
{
  switch (bpType) {
    case STATIC:
    case GSHARE:
      init_gshare();
      break;
    case TOURNAMENT:
      init_tournament();
      break;
    case CUSTOM:
      init_perceptron();
      break;
    default:
      break;
  }
  
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return gshare_predict(pc);
    case TOURNAMENT:
      return tournament_predict(pc);
    case CUSTOM:
      return perceptron_predict(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void
train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType) {
    case STATIC:
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      return train_perceptron(pc, outcome);
    default:
      break;
  }
  

}
