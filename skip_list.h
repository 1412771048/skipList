#include <stdint.h>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <stdio.h>
#include <mutex>


namespace skipList {

constexpr uint64_t MAX_LEVEL = 14;  //10万数据的合理层数
const std::string fileName = "kv.txt";

//后续看看小林
//节点间的跳转：同一个节点的多层跳转(用数组存储一个节点在不同层级的所有指针，有了指针就能知道它下一个是谁)、不同节点的单层跳转(单链表，指针实现)
//next把他当作是旋转了90度的哈希表来看, 层级从0层开始算，最底层为第0层
//跳表本质只有一层节点，上面的只是虚拟的，不占内存
template<typename K, typename V>
class Node {
public:
    Node() = default;
    Node(K k, V v, const uint64_t level): key(k), value(v), nodeLevel_(level), next(new Node<K, V>*[level + 1]{nullptr}) {}
    ~Node() {delete []next;}
    K get_key() const {return this->key;}
    V get_value() const {return this->value;}
    void set_value(V value) {this->value = value;}
    uint32_t get_nodeLevel() const {return this->nodeLevel_;}

    uint32_t nodeLevel_; //节点的层级: 如2,表示节点出现在2,1,0层    
    Node<K, V>** next;   //2级指针就是指针数组，含义：假设nodeLevel_=2,则next[2]表示该节点在第二层的下一个节点，next[1]表示该节点在第一层的下一个节点，next[0]指向该节点在第0层的下一个节点
private:
    K key;
    V value;
};

//SkipList类：操作Node类
template<typename K, typename V>
class SkipList{
public:
    SkipList();                                 // 构造函数
    ~SkipList();                                // 析构函数
    int get_random_level();                     // 获取节点的随机层级
    Node<K, V> *create_node(K, V, uint32_t);    // 节点创建
    int insert_element(K, V);                   // 插入节点
    void display_list();                        // 展示节点
    bool search_element(K);                     // 搜索节点
    void delete_element(K);                     // 删除节点
    void dump_file();                           // 持久化数据到文件
    void load_file();                           // 从文件加载数据
    void clear(Node<K, V> *);                   // 递归删除节点
    int size();                                 // 跳表中的节点个数
private:
    uint32_t maxLevel_;                         // 跳表允许的最大层数
    int currentLevel_;                          // 跳表当前的层数，初始化为-1，表示没有
    Node<K, V>* header_;                        // 跳表的头节点,也是入口，虚拟头节点
    int nodeCount_;                             // 跳表中组织的所有节点的数量
    std::mutex mtx_;
};

//随机选择层级：抛硬币：每当添加一个新元素，从第0层开始，正面上升一层，继续，反面停止，确定最终层级
//这个随机过程的结果就是：大部分元素会在低层级
//为啥要这样：随即确定层级开销小(红黑树还要旋转)；随机层级也能保证节点均匀分布，实现对数时间的增删改查; 跳表的优点就是简单高效
template<typename K, typename V>
int SkipList<K, V>::get_random_level() {
    uint32_t k = 0;
    while (rand() % 2 && k < maxLevel_) {
        ++k;
    }
    return k;
} 

//跳表初始化, 初始化为什么就有一个头节点了？这个应该叫虚拟头节点
template<typename K, typename V> 
SkipList<K, V>::SkipList(): maxLevel_(MAX_LEVEL), currentLevel_(-1), nodeCount_(0) {
    K k;
    V v;
    this->header_ = new Node<K, V>(k, v, MAX_LEVEL); //虚拟头节点的层级当然是最高层
}

//析构要释放最底层所有节点，包括虚拟头节点
template<typename K, typename V> 
SkipList<K, V>::~SkipList() {
    auto cur = this->header_->next[0];
    delete this->header_;
    while (cur) {
        auto next = cur->next[0];
        delete cur;
        cur = next;
    }
}


//创建节点
template<typename K, typename V> 
Node<K, V>* SkipList<K, V>::create_node(K k, V v, uint32_t level) {
    return new Node<K, V>(k, v, level);
}

//查找：从header_开始，这里假设是升序
template <typename K, typename V>
bool SkipList<K, V>::search_element(K key) {
    Node<K, V>* current = this->header_; 
    //搜最底层即可
    while (current->next[0] && current->next[0]->get_key() < key) {
        current = current->next[0]; // 移动到当前层的下一个节点
    }
    //出循环要么current->next[0]为空
    if (current->next[0] && current->next[0]->get_key() == key) {
        return true;
    }
    return false;
}

//插入：与搜索类似，只不过一边搜一边记录下每层可能后续要插入的位置
template <typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {
    std::lock_guard<std::mutex> lock(mtx_);

    Node<K, V>* update[MAX_LEVEL + 1] = {nullptr};  //记下每层可能要插入的位置, 下标即表示为哪一层

    //应该从最底层判断,搜索可能的插入位置，但记住：上面的层都是虚拟的，只是一个数组 
    for (int i = 0; i <= currentLevel_; i++) {
        Node<K, V>* current = this->header_;
        // 寻找当前层中最接近且小于key的节点
        while (current->next[i] && current->next[i]->get_key() < key) {
            current = current->next[i]; // 移动到下一节点
        }
        //第0层有，就不用插入了
        if (i == 0 && current->next[i] && current->next[i]->get_key() == key) {
            return 1;
        }
        //没有，记录每个要插入的位置的前节点
        update[i] = current; 
    }

    // 1. 更新跳表数据，并通过随机函数决定新节点的层级高度
    int random_level = get_random_level();
    if (random_level > MAX_LEVEL) return 1;

    this->nodeCount_++;
    if (random_level > currentLevel_) {
        //对于更高层，只能把虚拟头节点作为他的前节点
        for (int i = currentLevel_ + 1; i <= random_level; i++) {
            update[i] = header_;
        }
        currentLevel_ = random_level;//更新跳表层级
    }
    
    //2. 插入节点
    Node<K, V> *inserted_node = create_node(key, value, random_level);//创建节点
    //3. 更新跳表结构
    for (int i = 0; i <= random_level; i++) {//从最低层0层开始单链表的插入操作：更新前后节点指针
        inserted_node->next[i] = update[i]->next[i];//B->C
        update[i]->next[i] = inserted_node; //A->B
    }
    return 0;
}

//删除：与插入同理，把每层要删除的位置记录下来
template <typename K, typename V>
void SkipList<K, V>::delete_element(K key) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!search_element(key)) return;
    //节点存在
    Node<K, V>* res = nullptr;//这就是要删除的点
    Node<K, V>* update[MAX_LEVEL + 1] = {nullptr};  //记下每层要删除的位置
    int level = currentLevel_;
    
    for (int i = 0; i <= level; i++) {
        Node<K, V>* current = this->header_;
        while (current->next[i] && current->next[i]->get_key() != key) {
            current = current->next[i];
        }
        //出来后current->next[i]就指向待删除节点,肯定有，因为我知道它的层级啊
        if (i == 0) {
            level = current->next[0]->get_nodeLevel(); 
            res = current->next[0];
        } 
        update[i] = current;
    }

    //1. 调整层级
    for (int i = 0; i <= level; i++) {
        update[i]->next[i] = res->next[i];
    }
    //2. 删除节点,delete会先调用析构，释放该节点的数组，再free
    delete res;
    //3. 跟新跳表数据
    this->nodeCount_--;
    //从上往下看：删除此节点会不会导致那一层空了
    while (level >= 0 && !header_->next[level]) { 
        level--;
        currentLevel_--;
    }
}

//展示跳表
template <typename K, typename V>
void SkipList<K, V>::display_list() {
    //从最顶层开始遍历
    for (int i = currentLevel_; i >= 0; i--) {
        Node<K, V>* cur = this->header_->next[i]; //本层头节点
        std::cout << "Level " << i << ": ";
        while (cur) {
            std::cout << "[" << cur->get_key() << "," << cur->get_value() << "] ";
            cur = cur->next[i];
        }
        std::cout << std::endl;
    }
}

template <typename K, typename V>
void SkipList<K, V>::dump_file() {
    FILE* fp = fopen(fileName.c_str(), "w");//覆盖写入
    if (!fp) {
        std::cerr << "file: " << fileName << "open error!" << std::endl;
        return;
    }
    
    //遍历最底层，全部写入
    Node<K, V>* cur = this->header_->next[0];
    while (cur) {
        std::string tmp = std::to_string(cur->get_key()) + ":" + std::to_string(cur->get_value()) + "\n";
        fputs(tmp.c_str(), fp);
        cur = cur->next[0];
    }
    fclose(fp);
}

template <typename K, typename V>
void SkipList<K, V>::load_file() {
    //从文件解析键值对，插入到跳表
    FILE* fp = fopen(fileName.c_str(), "r");
    if (!fp) {
        std::cerr << "file: " << fileName << "open error!" << std::endl;
        return;
    }
    //循环读取
    char buf[1024]  ={0};
    while (!feof(fp) && !ferror(fp)) {
        //getline和fgets?????????????????????????
        fgets(buf, sizeof(buf), fp); 
        //分割字符串
        std::string str = buf;
        int idx = str.find(":");
        if (idx >= 0 && idx < str.size() - 1) {
            std::string key = str.substr(0, idx);
            std::string value = str.substr(idx + 1, str.size());
            value.erase(value.find_last_not_of("\n") + 1);
            std::cout << key << ":" << value << std::endl;
        }

    }
    fclose(fp);
}

} //namespace skipList