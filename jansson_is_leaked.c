#include <stdio.h>
#include <stdlib.h>
#include <jansson.h>

int main(void) {
    // 반복 횟수를 늘려서 Jansson 내부 캐시나 할당/해제가 올바르게 이루어지는지 관찰
    const int N = 1000;

    for (int i = 0; i < N; i++) {
        // 1) 최상위 JSON 객체 생성
        json_t *obj = json_object();
        if (!obj) {
            fprintf(stderr, "[ERROR] json_object() failed at iteration %d\n", i);
            return 1;
        }

        // 2) 정수 필드 추가: "number": i
        //    → json_integer()로 생성된 노드를 obj가 소유권을 가져감
        if (json_object_set_new(obj, "number", json_integer(i)) != 0) {
            fprintf(stderr, "[ERROR] json_object_set_new(number) failed at iteration %d\n", i);
            json_decref(obj);
            return 1;
        }

        // 3) 문자열 필드 추가: "message": "hello"
        if (json_object_set_new(obj, "message", json_string("hello")) != 0) {
            fprintf(stderr, "[ERROR] json_object_set_new(message) failed at iteration %d\n", i);
            json_decref(obj);
            return 1;
        }

        // 4) 배열 필드 생성: "values": [0, 1, 2, ..., 9]
        json_t *arr = json_array();
        if (!arr) {
            fprintf(stderr, "[ERROR] json_array() failed at iteration %d\n", i);
            json_decref(obj);
            return 1;
        }
        for (int j = 0; j < 10; j++) {
            // json_integer(j) 로 새 노드 생성 → json_array_append_new()이 소유권을 가져감
            if (json_array_append_new(arr, json_integer(j)) != 0) {
                fprintf(stderr, "[ERROR] json_array_append_new() failed at iteration %d, j=%d\n", i, j);
                json_decref(arr);
                json_decref(obj);
                return 1;
            }
        }
        // 생성한 배열을 객체에 붙이기
        if (json_object_set_new(obj, "values", arr) != 0) {
            fprintf(stderr, "[ERROR] json_object_set_new(values) failed at iteration %d\n", i);
            json_decref(arr);
            json_decref(obj);
            return 1;
        }

        // 5) JSON 문자열로 직렬화한 뒤 출력(선택)
        char *dump = json_dumps(obj, JSON_COMPACT);
        if (!dump) {
            fprintf(stderr, "[ERROR] json_dumps() failed at iteration %d\n", i);
            json_decref(obj);
            return 1;
        }
        // printf("%s\n", dump);  // 너무 많은 출력이 싫으면 주석 처리 가능
        free(dump);  // json_dumps()로 받은 메모리는 반드시 free()

        // 6) 최종적으로 obj만 json_decref 하면, obj 내부의 모든 child(JSON integer, string, array 등)도 재귀적으로 해제
        json_decref(obj);
    }

    return 0;
}
