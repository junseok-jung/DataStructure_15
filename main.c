#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 성능 측정 지표 구조체 */
typedef struct {
    double total_time_ms;
    long long comparisons;
    long long movements;
    size_t current_memory;
    size_t peak_memory;
    int final_count;
} Metrics;

/* 연산 정보 구조체 */
typedef struct {
    int is_insert; /* 1: 삽입(짝수), 0: 삭제(홀수) */
    int value;
} Operation;

/* AVL 트리 노드 구조체 */
typedef struct AVLNode {
    int key;
    int count;              /* 중복 데이터 개수 */
    int height;             /* 노드 높이 (제어용) */
    struct AVLNode* left;
    struct AVLNode* right;
} AVLNode;

/* 메모리 추적 헬퍼 함수 */
void add_memory(Metrics* m, size_t bytes) {
    m->current_memory += bytes;
    if (m->current_memory > m->peak_memory) {
        m->peak_memory = m->current_memory;
    }
}

void sub_memory(Metrics* m, size_t bytes) {
    if (m->current_memory >= bytes) {
        m->current_memory -= bytes;
    }
    else {
        m->current_memory = 0;
    }
}

/* =========================================================
 * 1. 퀵 정렬 및 정렬된 배열 기반 삽입·삭제
 * ========================================================= */

void swap_int(int* a, int* b, Metrics* m) {
    int temp = *a;
    *a = *b;
    *b = temp;
    m->movements += 3; /* 데이터 값 복사 및 대입 3회 */
}

/* 가운데 원소(Middle Element)를 피벗으로 선택하는 호어(Hoare) 분할 기반 퀵 정렬 */
void quick_sort(int arr[], int left, int right, Metrics* m) {
    if (left >= right) return;

    /* 재귀 호출 스택 프레임 메모리 반영 (left, right, i, j, pivot 등 약 32 Bytes) */
    size_t stack_frame = 32;
    add_memory(m, stack_frame);

    int i = left;
    int j = right;
    int pivot = arr[(left + right) / 2];
    m->movements += 1; /* 피벗 변수에 데이터 값 대입 */

    while (i <= j) {
        while (1) {
            m->comparisons++;
            if (arr[i] < pivot) i++;
            else break;
        }
        while (1) {
            m->comparisons++;
            if (arr[j] > pivot) j--;
            else break;
        }
        if (i <= j) {
            if (i != j) {
                swap_int(&arr[i], &arr[j], m);
            }
            i++;
            j--;
        }
    }

    if (left < j) quick_sort(arr, left, j, m);
    if (i < right) quick_sort(arr, i, right, m);

    sub_memory(m, stack_frame);
}

/* 정렬된 배열에 선형 탐색 후 삽입 */
void array_insert(int arr[], int* count, int key, Metrics* m) {
    int pos = 0;
    while (pos < *count) {
        m->comparisons++;
        if (arr[pos] < key) {
            pos++;
        }
        else {
            break;
        }
    }

    /* 기존 데이터를 뒤로 1칸씩 이동 */
    for (int i = *count; i > pos; i--) {
        arr[i] = arr[i - 1];
        m->movements++;
    }
    arr[pos] = key;
    m->movements++; /* 새 데이터 대입 */
    (*count)++;
}

/* 정렬된 배열에서 선형 탐색 후 단일 원소 삭제 */
void array_delete(int arr[], int* count, int key, Metrics* m) {
    int pos = -1;
    for (int i = 0; i < *count; i++) {
        m->comparisons++;
        if (arr[i] == key) {
            pos = i;
            break;
        }
        m->comparisons++;
        if (arr[i] > key) {
            /* 오름차순 정렬되어 있으므로 key보다 커지면 탐색 조기 종료 */
            break;
        }
    }

    /* 삭제할 값이 존재하지 않으면 다음 연산으로 진행 */
    if (pos == -1) return;

    /* 뒤의 데이터를 앞으로 1칸씩 이동 */
    for (int i = pos; i < *count - 1; i++) {
        arr[i] = arr[i + 1];
        m->movements++;
    }
    (*count)--;
}

/* =========================================================
 * 2. AVL 트리 정렬 및 삽입·삭제
 * ========================================================= */

int get_height(AVLNode* node) {
    return node ? node->height : 0;
}

int max_int(int a, int b) {
    return (a > b) ? a : b;
}

int get_balance(AVLNode* node) {
    return node ? (get_height(node->left) - get_height(node->right)) : 0;
}

AVLNode* create_node(int key, Metrics* m) {
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    add_memory(m, sizeof(AVLNode));

    node->key = key;
    m->movements += 1; /* 노드에 데이터 키 값 대입 */
    node->count = 1;
    node->height = 1;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/* 우회전 (LL 회전) - 포인터 연결 변경은 데이터 이동 횟수에서 제외 */
AVLNode* rotate_right(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = max_int(get_height(y->left), get_height(y->right)) + 1;
    x->height = max_int(get_height(x->left), get_height(x->right)) + 1;

    return x;
}

/* 좌회전 (RR 회전) - 포인터 연결 변경은 데이터 이동 횟수에서 제외 */
AVLNode* rotate_left(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = max_int(get_height(x->left), get_height(x->right)) + 1;
    y->height = max_int(get_height(y->left), get_height(y->right)) + 1;

    return y;
}

/* AVL 트리 노드 삽입 */
AVLNode* avl_insert(AVLNode* node, int key, Metrics* m) {
    if (node == NULL) {
        return create_node(key, m);
    }

    m->comparisons++;
    if (key < node->key) {
        node->left = avl_insert(node->left, key, m);
    }
    else {
        m->comparisons++;
        if (key > node->key) {
            node->right = avl_insert(node->right, key, m);
        }
        else {
            /* 중복 키인 경우 count만 증가 */
            node->count++;
            return node;
        }
    }

    /* 높이 갱신 및 균형 인수 계산 (제어 비교이므로 comparisons에서 제외) */
    node->height = 1 + max_int(get_height(node->left), get_height(node->right));
    int balance = get_balance(node);

    /* 불균형 발생 시 4가지 회전 수행 */
    if (balance > 1 && get_balance(node->left) >= 0) {
        return rotate_right(node); /* LL */
    }
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node); /* LR */
    }
    if (balance < -1 && get_balance(node->right) <= 0) {
        return rotate_left(node);  /* RR */
    }
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);  /* RL */
    }

    return node;
}

/* 우측 서브트리의 최소값 노드 탐색 */
AVLNode* min_value_node(AVLNode* node) {
    AVLNode* current = node;
    while (current->left != NULL) {
        current = current->left;
    }
    return current;
}

/* AVL 트리 노드 삭제 (단일 원소 제거 vs 노드 자체 제거 구분) */
AVLNode* avl_delete(AVLNode* root, int key, int remove_all, Metrics* m) {
    if (root == NULL) return root;

    m->comparisons++;
    if (key < root->key) {
        root->left = avl_delete(root->left, key, remove_all, m);
    }
    else {
        m->comparisons++;
        if (key > root->key) {
            root->right = avl_delete(root->right, key, remove_all, m);
        }
        else {
            /* 삭제 대상 키를 찾은 경우 */
            if (!remove_all && root->count > 1) {
                root->count--;
                return root;
            }

            /* 노드를 실제로 제거해야 하는 경우 */
            if ((root->left == NULL) || (root->right == NULL)) {
                AVLNode* temp = root->left ? root->left : root->right;
                if (temp == NULL) {
                    temp = root;
                    root = NULL;
                }
                else {
                    /* 자식 노드 내용을 현재 노드로 복사 (키 복사 1회 반영) */
                    *root = *temp;
                    m->movements += 1;
                }
                sub_memory(m, sizeof(AVLNode));
                free(temp);
            }
            else {
                /* 자식이 둘인 경우: 중위 후계자(Inorder Successor) 탐색 */
                AVLNode* temp = min_value_node(root->right);
                root->key = temp->key;
                root->count = temp->count;
                m->movements += 1; /* 후계자 키 값 복사 */

                /* 후계자 노드는 트리에서 완전히 제거 */
                root->right = avl_delete(root->right, temp->key, 1, m);
            }
        }
    }

    if (root == NULL) return root;

    /* 높이 갱신 및 균형 복구 */
    root->height = 1 + max_int(get_height(root->left), get_height(root->right));
    int balance = get_balance(root);

    if (balance > 1 && get_balance(root->left) >= 0)
        return rotate_right(root);
    if (balance > 1 && get_balance(root->left) < 0) {
        root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (balance < -1 && get_balance(root->right) <= 0)
        return rotate_left(root);
    if (balance < -1 && get_balance(root->right) > 0) {
        root->right = rotate_right(root->right);
        return rotate_left(root);
    }

    return root;
}

/* 중위 순회로 오름차순 결과 및 총 원소 개수 확인 */
void avl_inorder_count(AVLNode* root, int* total_count) {
    if (root == NULL) return;
    avl_inorder_count(root->left, total_count);
    *total_count += root->count;
    avl_inorder_count(root->right, total_count);
}

/* 메모리 해제 */
void free_avl(AVLNode* root) {
    if (root == NULL) return;
    free_avl(root->left);
    free_avl(root->right);
    free(root);
}

/* =========================================================
 * 3. 실험 실행 및 결과 출력
 * ========================================================= */

void run_experiment(int n) {
    int m_ops = n / 5; /* 최초 데이터 크기의 20% */
    int max_val = n;   /* 중복 및 삭제 적중이 발생하도록 0 ~ n-1 범위의 난수 사용 */

    /* 원본 데이터 및 연산 시퀀스 사전 생성 (측정 시간 제외) */
    int* orig_data = (int*)malloc(sizeof(int) * n);
    Operation* ops = (Operation*)malloc(sizeof(Operation) * m_ops);

    for (int i = 0; i < n; i++) {
        orig_data[i] = rand() % max_val;
    }
    for (int i = 0; i < m_ops; i++) {
        int r_type = rand();
        ops[i].is_insert = (r_type % 2 == 0) ? 1 : 0; /* 짝수: 삽입, 홀수: 삭제 */
        ops[i].value = rand() % max_val;              /* 삽입·삭제 대상 난수 별도 생성 */
    }

    /* -----------------------------------------
     * [1] 퀵 정렬 + 정렬된 배열 삽입·삭제 측정
     * ----------------------------------------- */
    Metrics qs_metrics = { 0, 0, 0, 0, 0, 0 };
    int max_capacity = n + m_ops;
    int* qs_arr = (int*)malloc(sizeof(int) * max_capacity);
    memcpy(qs_arr, orig_data, sizeof(int) * n);
    int qs_count = n;

    /* 배열 메모리 사용량 등록 */
    add_memory(&qs_metrics, sizeof(int) * max_capacity);

    clock_t start_qs = clock();
    quick_sort(qs_arr, 0, qs_count - 1, &qs_metrics);
    for (int i = 0; i < m_ops; i++) {
        if (ops[i].is_insert) {
            array_insert(qs_arr, &qs_count, ops[i].value, &qs_metrics);
        }
        else {
            array_delete(qs_arr, &qs_count, ops[i].value, &qs_metrics);
        }
    }
    clock_t end_qs = clock();
    qs_metrics.total_time_ms = (double)(end_qs - start_qs) * 1000.0 / CLOCKS_PER_SEC;
    qs_metrics.final_count = qs_count;

    /* -----------------------------------------
     * [2] AVL 트리 정렬 + 트리 삽입·삭제 측정
     * ----------------------------------------- */
    Metrics avl_metrics = { 0, 0, 0, 0, 0, 0 };
    AVLNode* root = NULL;

    clock_t start_avl = clock();
    for (int i = 0; i < n; i++) {
        root = avl_insert(root, orig_data[i], &avl_metrics);
    }
    for (int i = 0; i < m_ops; i++) {
        if (ops[i].is_insert) {
            root = avl_insert(root, ops[i].value, &avl_metrics);
        }
        else {
            root = avl_delete(root, ops[i].value, 0, &avl_metrics);
        }
    }
    clock_t end_avl = clock();
    avl_metrics.total_time_ms = (double)(end_avl - start_avl) * 1000.0 / CLOCKS_PER_SEC;

    int avl_total_count = 0;
    avl_inorder_count(root, &avl_total_count);
    avl_metrics.final_count = avl_total_count;

    /* -----------------------------------------
     * [3] 결과 출력
     * ----------------------------------------- */
    printf("Data Size : %d\n", n);
    printf("Operations: %d\n\n", m_ops);
    printf("                  %-15s %-15s\n", "Quick Sort", "AVL Tree");
    printf("------------------------------------------------\n");
    printf("Total Time        %-4.2f ms         %-4.2f ms\n", qs_metrics.total_time_ms, avl_metrics.total_time_ms);
    printf("Comparisons       %-15lld %-15lld\n", qs_metrics.comparisons, avl_metrics.comparisons);
    printf("Data Movements    %-15lld %-15lld\n", qs_metrics.movements, avl_metrics.movements);
    printf("Memory Usage      %-9zu Bytes %-9zu Bytes\n\n", qs_metrics.peak_memory, avl_metrics.peak_memory);
    printf("Final Data Count  %-15d %-15d\n\n\n", qs_metrics.final_count, avl_metrics.final_count);

    free(qs_arr);
    free_avl(root);
    free(orig_data);
    free(ops);
}

int main(void) {
    srand((unsigned int)time(NULL));
    int sizes[] = { 1000, 2000, 4000, 8000, 16000, 20000 };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < num_sizes; i++) {
        run_experiment(sizes[i]);
    }
    return 0;
}