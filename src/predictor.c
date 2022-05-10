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
int bpType;       // Branch Prediction Type
int verbose;



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

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

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
    default:
      break;
  }
  

}
