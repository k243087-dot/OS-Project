#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MAX_STUDENTS 15
#define NUM_QUESTIONS 3
#define HEADER_ROWS    6 

struct Question { 
    char text[100]; 
    char options[4][50];
    int answer; 
};

struct Student { int id, score, progress; bool isFinished; char status[20]; };

Question g_bank[NUM_QUESTIONS]; 
Student  g_board[MAX_STUDENTS]; 
int      g_numStudents;         
int      g_finishedCount = 0;   
int      g_totalScore = 0;      
bool     g_questionsSet = false;

pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER; 
sem_t           g_fileSem;                               
sem_t           g_computerSem;         

void gotoRow(int row) { printf("\033[%d;1H", row); }
void clearLine()      { printf("\033[K");           }

void adminPortal() {
    printf("\033[H\033[J"); 
    printf("********** ADMIN PORTAL: QUIZ SETUP **********\n");
    
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        printf("\n--- Question %d ---\n", i + 1);
        printf("Enter Question Text: ");
        scanf(" %99[^\n]", g_bank[i].text);
        
        for (int j = 0; j < 4; j++) {
            printf("  Enter Option %d: ", j + 1);
            scanf(" %49[^\n]", g_bank[i].options[j]);
        }
        
        printf("Set Correct Option Number (1-4): ");
        scanf("%d", &g_bank[i].answer);
    }
    
    g_questionsSet = true;
    printf("\nQuestions & Options saved! Press Enter to return to menu...");
    getchar(); getchar(); 
}

void* monitorTask(void*) {
    while (true) {
        gotoRow(1);
        printf("==============================================\n");
        printf("    LIVE MCQ QUIZ - COMPUTER LAB SYSTEM       \n");
        printf("==============================================\n");
        
        pthread_mutex_lock(&g_lock);
        int currentDone = g_finishedCount; 
        double average = (currentDone > 0) ? (double)g_totalScore / currentDone : 0;
        
        printf(" Completed: %d/%d  |  Class Average: %.2f\n", currentDone, g_numStudents, average);
        printf("----------------------------------------------\n");
        printf("%-4s| %-9s| %-7s| Status\n", "ID", "Progress", "Score");

        for (int i = 0; i < g_numStudents; i++) {
            gotoRow(HEADER_ROWS + 1 + i);
            clearLine();
            printf("%-4d| %d/%d         | %-7d| %s",
                g_board[i].id, g_board[i].progress, NUM_QUESTIONS,
                g_board[i].score, g_board[i].status);
        }
        pthread_mutex_unlock(&g_lock);

        fflush(stdout);
        if (currentDone >= g_numStudents) break; 
        usleep(150000); 
    }
    return NULL;
}

void* studentTask(void* arg) {
    int idx = *(int*)arg;
    pthread_mutex_lock(&g_lock);
    sprintf(g_board[idx].status, "Waiting... 🕒");
    pthread_mutex_unlock(&g_lock);
    
    sem_wait(&g_computerSem); 

    for (int q = 0; q < NUM_QUESTIONS; q++) {
        pthread_mutex_lock(&g_lock);
        sprintf(g_board[idx].status, "Thinking ⏳");
        pthread_mutex_unlock(&g_lock);
        
        usleep(1000000 + rand() % 1500000); 

        pthread_mutex_lock(&g_lock); 
        sprintf(g_board[idx].status, "Answering ✍️");
        
        int correctAnswer = g_bank[q].answer;
        int studentChoiceIdx = rand() % 4;
        int studentAnsNum = studentChoiceIdx + 1;
        
        bool isCorrect = (studentAnsNum == correctAnswer);
        if (isCorrect) g_board[idx].score += 20;
        g_board[idx].progress++;
        pthread_mutex_unlock(&g_lock); 

        sem_wait(&g_fileSem);
        std::ofstream f("quiz_results.txt", std::ios::app);
        if (f.is_open()) {
            f << "Student " << g_board[idx].id << " chose: \"" 
              << g_bank[q].options[studentChoiceIdx] << "\"" 
              << " for Q" << (q+1) << (isCorrect ? " (CORRECT)" : " (WRONG)") << "\n";
            f.close();
        }
        sem_post(&g_fileSem);
        usleep(400000); 
    }

    pthread_mutex_lock(&g_lock);
    sprintf(g_board[idx].status, "FINISHED ✅");
    g_finishedCount++; 
    g_totalScore += g_board[idx].score; 
    pthread_mutex_unlock(&g_lock);

    sem_post(&g_computerSem); 
    return NULL;
}

void studentPortal() {
    if (!g_questionsSet) {
        printf("\n[ERROR] No questions found! Please visit Admin Portal first.\n");
        sleep(2);
        return;
    }

    printf("\nHow many students are taking the quiz? (1-%d): ", MAX_STUDENTS);
    scanf("%d", &g_numStudents);
    if (g_numStudents > MAX_STUDENTS) g_numStudents = MAX_STUDENTS;

    g_finishedCount = 0;
    g_totalScore = 0;
    sem_init(&g_fileSem, 0, 1);
    sem_init(&g_computerSem, 0, 3); 
    fclose(fopen("quiz_results.txt", "w")); 

    printf("\033[H\033[J");

    pthread_t students[MAX_STUDENTS], monitor;
    int indices[MAX_STUDENTS];

    pthread_create(&monitor, NULL, monitorTask, NULL);

    for (int i = 0; i < g_numStudents; i++) {
        indices[i] = i;
        g_board[i] = {i + 1, 0, 0, false, "Queued..."};
        pthread_create(&students[i], NULL, studentTask, &indices[i]);
    }

    int localFinished = 0;
    while (localFinished < g_numStudents) {
        pthread_mutex_lock(&g_lock);     
        localFinished = g_finishedCount; 
        pthread_mutex_unlock(&g_lock);   
        usleep(100000);                  
    }

    pthread_join(monitor, NULL);
    for (int i = 0; i < g_numStudents; i++) pthread_join(students[i], NULL);
    
    sem_destroy(&g_fileSem);
    sem_destroy(&g_computerSem);

    gotoRow(HEADER_ROWS + g_numStudents + 2);
    printf("Simulation Complete. Results saved to quiz_results.txt\n");
    printf("Press Enter to return to menu...");
    getchar(); getchar();
}

int main() {
    srand(time(NULL));
    int choice;

    while (true) {
        printf("\033[H\033[J"); 
        printf("========== MULTI-THREADED QUIZ SYSTEM ==========\n");
        printf("1. Admin Portal (Set Questions & Options)\n");
        printf("2. Student Portal (Run Simulation)\n");
        printf("3. Exit\n");
        printf("================================================\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        if (choice == 1) adminPortal();
        else if (choice == 2) studentPortal();
        else if (choice == 3) break;
    }

    return 0;
}
