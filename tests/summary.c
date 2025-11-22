#include <stdio.h>
#include <stdlib.h>
#include "../src/db.h"
#include "../src/error.h"

typedef struct {
    char id[32];
    char name[128];
    char programme[128];
    float mark;
} StudentRecord;


void showSummary(StudentRecord records[], int count) {
    if (count == 0) {
        printf("No records available.\n");
        return;
    }
    int total = count;
    float sum = 0.0;

    // Assign highest & lowest marks to mark of first student record
    float highest = records[0].mark;
    float lowest = records[0].mark;

    // Allocate arrays to hold indices of students with highest and lowest marks
    // This allocates memory enough to store an index for each student, in case multiple students share highest/lowest mark
    int* highest_indices = malloc(count * sizeof(int)); 
    int* lowest_indices = malloc(count * sizeof(int));

    // Counters for the number of students sharing the highest or lowest mark
    int highCount = 1, lowCount = 1; 
    
    // Since first student record is considered initial highest and lowest, store index 0 in both arrays
    highest_indices[0] = 0;
    lowest_indices[0] = 0;

    for (int i = 1; i < count; ++i) {
        float mark = records[i].mark;
        sum += mark;

        // Check if current mark is greater than the recorded highest mark
        if (mark > highest) {
            highest = mark;
            highCount = 1;
            highest_indices[0] = i;
        } else if (mark == highest) {
            highest_indices[highCount++] = i;
        }

        // Check if current mark is greater than the recorded lowest mark
        if (mark < lowest) {
            lowest = mark;
            lowCount = 1;
            lowest_indices[0] = i;
        } else if (mark == lowest) {
            lowest_indices[lowCount++] = i;
        }
    }

    float avg = sum / total;
    printf("\nTotal number of students: %d\n", total);
    printf("Average mark: %.1f\n", avg);
    printf("Highest mark: %.1f by", highest);
    for (int i = 0; i < highCount; ++i) {
        printf(" %s", records[highest_indices[i]].name);
        if (i < highCount - 1) printf(",");
    }
    printf("\n");

    printf("Lowest mark: %.1f by", lowest);
    for (int i = 0; i < lowCount; ++i) {
        printf(" %s", records[lowest_indices[i]].name);
        if (i < lowCount - 1) printf(",");
    }
    printf("\n\n");

    // Free dynamically allocated memory to avoid memory leaks
    free(highest_indices);
    free(lowest_indices);
}

int main() {
    StudentRecord records[] = {
        {"1", "Tosh", "Computer Science", 88.8},
        {"2", "Castorice", "Mathematics", 77.0},
        {"3", "Phainon", "Physics", 67.0},
        {"4", "Anaxagoras", "Chemistry", 100.0},
        {"5", "Mydei", "History", 99.5}
    };
    int count = sizeof(records) / sizeof(records[0]);
    showSummary(records, count);
    return 0;
}