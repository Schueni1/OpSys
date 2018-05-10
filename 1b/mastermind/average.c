#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define TESTSIZE (32768)
#define PARITY_ERR_BIT (6)
#define SHIFT_WIDTH (3)
#define MAX_TRIES (35)
#define SLOTS (5)
#define COLORS (8)
#define COMBINATIONS (32768)

#define BUFFER_BYTES (2)
#define EXIT_PARITY_ERROR (2)
#define EXIT_GAME_LOST (3)
#define EXIT_MULTIPLE_ERRORS (4)

#define BACKLOG (5)

uint16_t compute_patern(void);

uint16_t allCombinations[COMBINATIONS];
int red, white;

void compute_all_combinations(void);

void exclude_combinations(uint8_t *buff, uint8_t *prev_guess);

int check_answer(uint16_t req, uint8_t *resp, uint8_t *secret);


int main(void) {
    bool found = false;
    int max = 0, sum = 0;
    int games = 0;
    int neuner = 0;
    int achter = 0;
    uint16_t hardestComb;
    for(uint16_t s = 0; s < TESTSIZE; ++s) {
      //uint16_t secret = rand() % COMBINATIONS;
      uint16_t secret = s;
      (void)compute_all_combinations();
        int i = 1;
        found = false;
        for(; i < MAX_TRIES; ++i) {
            uint16_t try = compute_patern();
            uint8_t secretAsArray[SLOTS];
            uint8_t buff[BUFFER_BYTES];
            uint16_t value = try;
            uint16_t tempSecret = secret;
            buff[0] = value & 0xff;
            buff[1] = (value >> 8) & 0xff;
            for (int j = 0; j < SLOTS; ++j) {
                int tmp = tempSecret & 0x7;
                secretAsArray[j] = tmp;
                tempSecret >>= SHIFT_WIDTH;
            }
            int c = check_answer(try, buff, secretAsArray);
            red = (buff[0] & 7);
            white = (buff[0] >> 3) & 7;
            if(c==5) {
                found = true;
                break;
            }
            uint8_t prev_Try[SLOTS];
            for (int j = 0; j < SLOTS; ++j) {
                int tmp = try & 0x7;
                prev_Try[j] = tmp;
                try >>= SHIFT_WIDTH;
            }
            exclude_combinations(buff, prev_Try);
        }
        if(found==false){
          break;
        }
        if(i==8){
          ++achter;
        }
        else if(i==9){
          ++neuner;
        }
        else if(i==10){
          ++games;
        }
        if(max < i) {
            hardestComb = secret;
            max = i;
        }
        sum += i;
    }
    if(found == false){
      printf("not found");
    }
    uint8_t prev_try[SLOTS];
    for (int j = 0; j < SLOTS; ++j) {
        int tmp = hardestComb & 0x7;
        prev_try[j] = tmp;
        hardestComb >>= SHIFT_WIDTH;
    }
    for (size_t p = 0; p < 5; p++) {
      printf("%d ", prev_try[p]);
    }
    printf("\n");
    printf("Anzahl 8er: %d\n", achter);
    printf("Anzahl 9er: %d\n", neuner);
    printf("Anzahl 10er: %d\n", games);
    printf("Max: %d \tAvg %lf\tTotal: %d\n", max, ((double)sum)/TESTSIZE, sum);
    return 0;
}

void compute_all_combinations(void) {
  for (uint16_t i = 0; i < COMBINATIONS; i++) {
    allCombinations[i] = i;
  }
}

int check_answer(uint16_t req, uint8_t *resp, uint8_t *secret){
    int colors_left[COLORS];
    int guess[COLORS];
    int red_temp, white_temp;
    int j;


    /* extract the guess and calculate parity */
    for (j = 0; j < SLOTS; ++j) {
        int tmp = req & 0x7;
        guess[j] = tmp;
        req >>= SHIFT_WIDTH;
    }

    /* marking red and white */
    (void) memset(&colors_left[0], 0, sizeof(colors_left));
    red_temp = white_temp = 0;
    for (j = 0; j < SLOTS; ++j) {
        /* mark red */
        if (guess[j] == secret[j]) {
            red_temp++;
        } else {
            colors_left[secret[j]]++;
        }
    }
    for (j = 0; j < SLOTS; ++j) {
        /* not marked red */
        if (guess[j] != secret[j]) {
            if (colors_left[guess[j]] > 0) {
                white_temp++;
                colors_left[guess[j]]--;
            }
        }
    }

    /* build response buffer */

    return red_temp;

}

void exclude_combinations(uint8_t *buff, uint8_t *prev_guess){
  int red_temp;
  int white_temp;
  int count = 0;
  for (size_t i = 0; i < COMBINATIONS; i++) {
    if(allCombinations[i] < COMBINATIONS){
      (void)check_answer(allCombinations[i],buff, prev_guess);
      red_temp = (buff[0] & 7);
      white_temp = (buff[0] >> 3) & 7;
      if(white_temp!=white || red_temp!=red ) {
        allCombinations[i] ^= (1 << 15);
      }
    }
    else{
      ++count;
    }
  }
}

uint16_t compute_patern(void) {
  int bestCount = 0;
  uint16_t selected_colors;
    for (size_t i = 0; i < COMBINATIONS; i++) {
      if(allCombinations[i] < COMBINATIONS){
        uint8_t possibleGuess[SLOTS];
        uint8_t countCoulors[COLORS];
        uint8_t parity_calc = 0;
        uint16_t selected_colors_temp = allCombinations[i];
        for (size_t j = 0; j < COLORS; j++) {
          countCoulors[j] = 16;
        }
        int count = 0;
        for (size_t j = 0; j < SLOTS; ++j) {
            int tmp = selected_colors_temp & 0x7;
            parity_calc ^= tmp ^ (tmp >> 1) ^ (tmp >> 2);
            possibleGuess[j] = tmp;
            if(countCoulors[possibleGuess[j]] != possibleGuess[j]){
              countCoulors[possibleGuess[j]] = possibleGuess[j];
              ++count;
              if(count>bestCount){
                bestCount = count;
              }
            }
            selected_colors_temp >>= SHIFT_WIDTH;
        }
        if(count >= bestCount){
          selected_colors = allCombinations[i];
        }
        if(bestCount>=4){
          break;
        }
      }
    }
    /* calculate parity */
    uint8_t parity_calc = 0;
    int tmp = selected_colors;

    for (int j = 0; j < 15; ++j) {
        parity_calc ^= tmp;
        tmp >>= 0x1;
    }
    parity_calc &= 0x1;

    return (selected_colors & 0x7FFF) | (parity_calc << 15);
}
