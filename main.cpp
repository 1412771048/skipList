#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include "skip_list.h"


int main() {
    //在堆区创建，因为你也不知道跳表有多大呢
    // skipList::SkipList<int, int> *skip_list = new skipList::SkipList<int, int>();
    auto skip_list = std::make_unique<skipList::SkipList<int, int>>();
    for (int i = 0; i < 1000; i++) {
        skip_list->insert_element(i, i);
    }

    auto time1= std::chrono::high_resolution_clock::now();
    skip_list->dump_file();
    auto time2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time2 - time1).count();
    std::cout << "Serialization to disk took " << duration << " milliseconds." << std::endl;


    // for (int i = 0, j = 0; i < 5; i++, j++) {
    //     if (skip_list->insert_element(i, j) == 0) {
    //         std::cout << "Insert Success" << std::endl;
    //     } else {
    //         std::cout << "Insert Failed" << std::endl;
    //     }
    // }


    // skip_list->display_list();
    // skip_list->dump_file();

    // skip_list->load_file();

    // for (int i = 0; i < 3; i++) {
    //     skip_list->delete_element(i);
    // }

    // // 搜索
    // for (int i = 0; i < 5; i++) {
    //     if (skip_list->search_element(i)) {
    //         std::cout << "key:" << i << " exist" << std::endl;
    //     } else {
    //         std::cout << "key:" << i << " not exist" << std::endl;
    //     }
    // }
    // skip_list->display_list();
}