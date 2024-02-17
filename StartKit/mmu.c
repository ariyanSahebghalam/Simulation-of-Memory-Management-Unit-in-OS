#include <stdio.h>
#include <stdlib.h>

#define PAGETABLE_SIZE 256
#define FRAMENUM 256
#define FRAMESIZE 256
#define PAGESIZE 256
#define TLBSIZE 16

// Each entry in the page table
typedef struct {
    int frameVal;
    int lastPage;
    int isValid;
} pageTable_entry;

// Array of page entries
pageTable_entry page_table_array[PAGETABLE_SIZE];   // There are 2^8 page entries in the page table array

// Each frame as a node in a linked list
typedef struct frameN {
    int frameNum;
    struct frameN *next;                            // Pointer to the value of same type, a node of the frame
} frameNode;

frameNode *head = NULL;                             // Pointer to the first frame

// Translation Lookaside Buffer entries
typedef struct {
    int pageNum;
    int frameNum;
} tlbEntry;

// Our translation lookaside buffer array
tlbEntry tlbArray[TLBSIZE];

// Function prototypes
int pageTable_search(int pageNumber);
int tlb_search(int pageNumber);

// Array of chars, physical memory
char phys_memory[FRAMESIZE * FRAMENUM];
char buffer[50];

int mem_access_counter = 0;
int pageFault = 0;
int tlbHit = 0;
int tlb_currentSize = 0;
int tlbIndex = 0;
int physicalAddress;
int frameValue;

// Main function
int main(int argc, char *argv[]) {
    // File pointer
    FILE *output;

    // Opening the file
    if (atoi(argv[1]) == 256) {
        output = fopen("output256.csv", "w"); // Output file for phase 1
    } else if (atoi(argv[1]) == 128) {
        output = fopen("output128.csv", "w"); // Output file for phase 2
    }

    FILE *input = fopen(argv[3], "r"); // Input file

    // Initializing all the values inside the page table to -1
    for (int i = 0; i < PAGETABLE_SIZE; i++) {
        page_table_array[i].isValid = -1;
    }

    // Size 128 or 256 in this case
    int free_frame_list_size = atoi(argv[1]);

    // Initializing the TLB, size 16
    for (int i = 0; i < TLBSIZE; i++) {
        tlbArray[i].pageNum = -1;
        tlbArray[i].frameNum = -1;
    }

    // Initializing the free frame list of nodes
    for (int i = free_frame_list_size - 1; i >= 0; i--) {
        frameNode *newNode = malloc(sizeof(frameNode));
        newNode->frameNum = i;
        newNode->next = (struct frameN *)head;
        head = newNode;
    }

      // Initiatlizing all the values according to the proper value, so the formula is
    // frame number * 256 + offset because you want a specific byte of the frame which is 256,
    // then you assign it to the page
    // For loop for extracting the logical addresses line by line
    while (fgets(buffer, sizeof(buffer), input) != NULL) {
        int logicalAddress = atoi(buffer);                   // Converting address to integer
        int pageNumber = (logicalAddress >> 8) & 0xFF;      // Extracting the second rightmost 8 bits
        int pageOffset = logicalAddress & 0xFF;             // Extracting the rightmost 8 bits

        // printf("Logical Address: %-10d pg offset:%-7d  pg num: %-7d\n", logicalAddress, pageOffset, pageNumber);
        mem_access_counter++;

        // If it is invalid, then you are allowed to add the new page, and make it valid
        // If the pageNumber is not occupied, then add the proper frame value to it
        // if (page_table_array[pageNumber].isValid == -1) {
        //     page_table_array[pageNumber].frameVal = curFrameCounter * FRAMESIZE;
        //     curFrameCounter++;
        //     page_table_array[pageNumber].isValid = 1;
        // }
        frameValue = tlb_search(pageNumber);

        // If TLB hit else miss
        if (frameValue != -1) {
        } else {

            // You gotta search the page table for each entry's page number
            frameValue = pageTable_search(pageNumber);


            // If frame not found, then value of the frame is -1, so make it 1, and page fault, refer to backing store to fill the value
            // If found, then return the frame value
            if (frameValue == -1) {
                if (!head) {
                    frameValue = -1;
                }
                else {
                    frameNode *tmp = head;
                    head = (frameNode *) head->next;
                    frameValue = tmp->frameNum;
                    free(tmp);
                }

                if (frameValue == -1) {
                    int validIndex = -1; // Initializing with an invalid inde

                    for (int i = 0; i < PAGETABLE_SIZE && validIndex == -1; i++) {
                        if (page_table_array[i].isValid == 1) {
                            validIndex = i;
                        }
                    }

                    int min = page_table_array[validIndex].lastPage;
                    int lru_frameNum = page_table_array[validIndex].frameVal;
                    int lru_pageNum = validIndex;

                    for (int i = 1; i < PAGETABLE_SIZE; i++) {
                        if (page_table_array[i].isValid == 1 && (page_table_array[i].lastPage < min)) {
                            lru_frameNum = page_table_array[i].frameVal;
                            lru_pageNum = i;
                            min = page_table_array[i].lastPage;
                        }
                    }

                    frameValue = lru_frameNum;
                    page_table_array[lru_pageNum].isValid = -1;
                }

                // Reading from the backing store
                FILE *bk_ptr = fopen(argv[2], "rb");
                fseek(bk_ptr, PAGESIZE * pageNumber, SEEK_SET);
                fread(phys_memory + (frameValue * FRAMESIZE), 1, PAGESIZE, bk_ptr);
                fclose(bk_ptr);
                page_table_array[pageNumber].lastPage = mem_access_counter;
                page_table_array[pageNumber].isValid = 1;
                page_table_array[pageNumber].frameVal = frameValue;
            }

            // FIFO TLB implementation
            tlb_currentSize++;
            tlbIndex = tlb_currentSize % TLBSIZE;
            tlbArray[tlbIndex].frameNum = frameValue;
            tlbArray[tlbIndex].pageNum = pageNumber;
            page_table_array[pageNumber].lastPage = mem_access_counter;
        }

        physicalAddress = frameValue * FRAMESIZE + pageOffset;

        // Putting the value inside 
        fprintf(output, "%d,%d,%d\n", logicalAddress, physicalAddress, phys_memory[physicalAddress]);
    }

    fprintf(output, "Page Faults Rate, %.2f%%,\n", ((double)pageFault / mem_access_counter) * 100);
    fprintf(output, "TLB Hits Rate, %.2f%%,", ((double)tlbHit / mem_access_counter) * 100);

    fclose(input);
    fclose(output);
    return 0;
}

// If found, return the frame number, else return -1
int pageTable_search(int pageNumber) {
    if (page_table_array[pageNumber].isValid == 1) {
        page_table_array[pageNumber].lastPage = mem_access_counter;
        return page_table_array[pageNumber].frameVal;
    }
    pageFault++;
    return -1;
}

// If found, return the frame number, else return -1
int tlb_search(int pageNumber) {
    for (int i = 0; i < TLBSIZE; i++) {
        if (tlbArray[i].pageNum == pageNumber) {
            tlbHit++;
            page_table_array[pageNumber].lastPage = mem_access_counter;
            return tlbArray[i].frameNum;
        }
    }
    return -1;
}