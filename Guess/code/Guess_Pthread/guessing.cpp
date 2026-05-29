#include "PCFG.h"
#include <pthread.h>
using namespace std;


// 线程函数声明
void* GenerateThreadFunc(void* arg);

// 线程函数实现
void* GenerateThreadFunc(void* arg) {
    ThreadParam_t* param = (ThreadParam_t*)arg;
    segment* a = param->seg;
    
    // 每个线程有自己的局部结果向量
    vector<string> local_vec;
    int local_cnt = 0;
    
    // 处理分配给本线程的索引范围
    for (int i = param->start; i < param->end; i++) {
        local_vec.emplace_back(a->ordered_values[i]);
        local_cnt++;
    }
    
    // 将结果存储到线程私有空间
    *(param->local_guesses) = std::move(local_vec);
    *(param->local_count) = local_cnt;
    
    pthread_exit(NULL);
}




void PriorityQueue::CalProb(PT &pt)
{
    // 计算PriorityQueue里面一个PT的流程如下：
    // 1. 首先需要计算一个PT本身的概率。例如，L6S1的概率为0.15
    // 2. 需要注意的是，Queue里面的PT不是“纯粹的”PT，而是除了最后一个segment以外，全部被value实例化的PT
    // 3. 所以，对于L6S1而言，其在Queue里面的实际PT可能是123456S1，其中“123456”为L6的一个具体value。
    // 4. 这个时候就需要计算123456在L6中出现的概率了。假设123456在所有L6 segment中的概率为0.1，那么123456S1的概率就是0.1*0.15

    // 计算一个PT本身的概率。后续所有具体segment value的概率，直接累乘在这个初始概率值上
    pt.prob = pt.preterm_prob;

    // index: 标注当前segment在PT中的位置
    int index = 0;


    for (int idx : pt.curr_indices)
    {
        // pt.content[index].PrintSeg();
        if (pt.content[index].type == 1)
        {
            // 下面这行代码的意义：
            // pt.content[index]：目前需要计算概率的segment
            // m.FindLetter(seg): 找到一个letter segment在模型中的对应下标
            // m.letters[m.FindLetter(seg)]：一个letter segment在模型中对应的所有统计数据
            // m.letters[m.FindLetter(seg)].ordered_values：一个letter segment在模型中，所有value的总数目
            pt.prob *= m.letters[m.FindLetter(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.letters[m.FindLetter(pt.content[index])].total_freq;
            // cout << m.letters[m.FindLetter(pt.content[index])].ordered_freqs[idx] << endl;
            // cout << m.letters[m.FindLetter(pt.content[index])].total_freq << endl;
        }
        if (pt.content[index].type == 2)
        {
            pt.prob *= m.digits[m.FindDigit(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.digits[m.FindDigit(pt.content[index])].total_freq;
            // cout << m.digits[m.FindDigit(pt.content[index])].ordered_freqs[idx] << endl;
            // cout << m.digits[m.FindDigit(pt.content[index])].total_freq << endl;
        }
        if (pt.content[index].type == 3)
        {
            pt.prob *= m.symbols[m.FindSymbol(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.symbols[m.FindSymbol(pt.content[index])].total_freq;
            // cout << m.symbols[m.FindSymbol(pt.content[index])].ordered_freqs[idx] << endl;
            // cout << m.symbols[m.FindSymbol(pt.content[index])].total_freq << endl;
        }
        index += 1;
    }
    // cout << pt.prob << endl;
}

void PriorityQueue::init()
{
    // cout << m.ordered_pts.size() << endl;
    // 用所有可能的PT，按概率降序填满整个优先队列
    for (PT pt : m.ordered_pts)
    {
        for (segment seg : pt.content)
        {
            if (seg.type == 1)
            {
                // 下面这行代码的意义：
                // max_indices用来表示PT中各个segment的可能数目。例如，L6S1中，假设模型统计到了100个L6，那么L6对应的最大下标就是99
                // （但由于后面采用了"<"的比较关系，所以其实max_indices[0]=100）
                // m.FindLetter(seg): 找到一个letter segment在模型中的对应下标
                // m.letters[m.FindLetter(seg)]：一个letter segment在模型中对应的所有统计数据
                // m.letters[m.FindLetter(seg)].ordered_values：一个letter segment在模型中，所有value的总数目
                pt.max_indices.emplace_back(m.letters[m.FindLetter(seg)].ordered_values.size());
            }
            if (seg.type == 2)
            {
                pt.max_indices.emplace_back(m.digits[m.FindDigit(seg)].ordered_values.size());
            }
            if (seg.type == 3)
            {
                pt.max_indices.emplace_back(m.symbols[m.FindSymbol(seg)].ordered_values.size());
            }
        }
        pt.preterm_prob = float(m.preterm_freq[m.FindPT(pt)]) / m.total_preterm;
        // pt.PrintPT();
        // cout << " " << m.preterm_freq[m.FindPT(pt)] << " " << m.total_preterm << " " << pt.preterm_prob << endl;

        // 计算当前pt的概率
        CalProb(pt);
        // 将PT放入优先队列
        priority.emplace_back(pt);
    }
    // cout << "priority size:" << priority.size() << endl;
}

void PriorityQueue::PopNext()
{

    // 对优先队列最前面的PT，首先利用这个PT生成一系列猜测
    Generate(priority.front());

    // 然后需要根据即将出队的PT，生成一系列新的PT
    vector<PT> new_pts = priority.front().NewPTs();
    for (PT pt : new_pts)
    {
        // 计算概率
        CalProb(pt);
        // 接下来的这个循环，作用是根据概率，将新的PT插入到优先队列中
        for (auto iter = priority.begin(); iter != priority.end(); iter++)
        {
            // 对于非队首和队尾的特殊情况
            if (iter != priority.end() - 1 && iter != priority.begin())
            {
                // 判定概率
                if (pt.prob <= iter->prob && pt.prob > (iter + 1)->prob)
                {
                    priority.emplace(iter + 1, pt);
                    break;
                }
            }
            if (iter == priority.end() - 1)
            {
                priority.emplace_back(pt);
                break;
            }
            if (iter == priority.begin() && iter->prob < pt.prob)
            {
                priority.emplace(iter, pt);
                break;
            }
        }
    }

    // 现在队首的PT善后工作已经结束，将其出队（删除）
    priority.erase(priority.begin());
}

// 这个函数你就算看不懂，对并行算法的实现影响也不大
// 当然如果你想做一个基于多优先队列的并行算法，可能得稍微看一看了
vector<PT> PT::NewPTs()
{
    // 存储生成的新PT
    vector<PT> res;

    // 假如这个PT只有一个segment
    // 那么这个segment的所有value在出队前就已经被遍历完毕，并作为猜测输出
    // 因此，所有这个PT可能对应的口令猜测已经遍历完成，无需生成新的PT
    if (content.size() == 1)
    {
        return res;
    }
    else
    {
        // 最初的pivot值。我们将更改位置下标大于等于这个pivot值的segment的值（最后一个segment除外），并且一次只更改一个segment
        // 上面这句话里是不是有没看懂的地方？接着往下看你应该会更明白
        int init_pivot = pivot;

        // 开始遍历所有位置值大于等于init_pivot值的segment
        // 注意i < curr_indices.size() - 1，也就是除去了最后一个segment（这个segment的赋值预留给并行环节）
        for (int i = pivot; i < curr_indices.size() - 1; i += 1)
        {
            // curr_indices: 标记各segment目前的value在模型里对应的下标
            curr_indices[i] += 1;

            // max_indices：标记各segment在模型中一共有多少个value
            if (curr_indices[i] < max_indices[i])
            {
                // 更新pivot值
                pivot = i;
                res.emplace_back(*this);
            }

            // 这个步骤对于你理解pivot的作用、新PT生成的过程而言，至关重要
            curr_indices[i] -= 1;
        }
        pivot = init_pivot;
        return res;
    }

    return res;
}


// 这个函数是PCFG并行化算法的主要载体
// 尽量看懂，然后进行并行实现


// ====== pthread 优化后的 ========
void PriorityQueue::Generate(PT pt)
{
    CalProb(pt);

    if (pt.content.size() == 1)
    {
        segment *a;
        if (pt.content[0].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[0])];
        }
        if (pt.content[0].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[0])];
        }
        if (pt.content[0].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[0])];
        }
        
        // ========== Pthread 并行化实现 ==========
        int total_values = pt.max_indices[0];
        
        // 设置线程数（可以根据CPU核心数调整，鲲鹏服务器建议使用7个子线程+主线程）
        // 由于主线程不参与计算，这里创建NUM_THREADS个工作线程
        int NUM_THREADS = 7;  // 鲲鹏服务器8核，主线程负责控制，7个工作线程
        
        // 如果数据量太小，直接串行执行避免线程开销
        if (total_values < 1000 || NUM_THREADS <= 1) {
            for (int i = 0; i < total_values; i += 1) {
                string guess = a->ordered_values[i];
                guesses.emplace_back(guess);
                total_guesses += 1;
            }
        } else {
            // 计算每个线程的负载（均分，最后线程处理剩余部分）
            int chunk_size = total_values / NUM_THREADS;
            int remainder = total_values % NUM_THREADS;
            
            // 创建线程数组和参数数组
            pthread_t threads[NUM_THREADS];
            ThreadParam_t params[NUM_THREADS];
            
            // 线程私有结果
            vector<string> local_results[NUM_THREADS];
            int local_counts[NUM_THREADS];
            
            int current_start = 0;
            
            // 创建并启动线程
            for (int t = 0; t < NUM_THREADS; t++) {
                int thread_end = current_start + chunk_size;
                if (t < remainder) {
                    thread_end += 1;  // 前remainder个线程多分配一个任务
                }
                
                params[t].seg = a;
                params[t].start = current_start;
                params[t].end = thread_end;
                params[t].local_guesses = &local_results[t];
                params[t].local_count = &local_counts[t];
                
                pthread_create(&threads[t], NULL, GenerateThreadFunc, (void*)&params[t]);
                
                current_start = thread_end;
            }
            
            // 等待所有线程完成并合并结果
            for (int t = 0; t < NUM_THREADS; t++) {
                pthread_join(threads[t], NULL);
                
                // 将线程私有结果合并到全局结果
                for (const string& guess : local_results[t]) {
                    guesses.emplace_back(guess);
                }
                total_guesses += local_counts[t];
            }
        }
    }
    else
    {
        string guess;
        int seg_idx = 0;
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1)
            {
                guess += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 2)
            {
                guess += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 3)
            {
                guess += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            }
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1)
            {
                break;
            }
        }

        segment *a;
        if (pt.content[pt.content.size() - 1].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
        }
        
        // ========== Pthread 并行化实现 ==========
        int total_values = pt.max_indices[pt.content.size() - 1];
        
        // 设置线程数
        int NUM_THREADS = 7;
        
        // 保存前缀字符串的副本，供所有线程使用
        string prefix = guess;
        
        // 如果数据量太小，直接串行执行
        if (total_values < 1000 || NUM_THREADS <= 1) {
            for (int i = 0; i < total_values; i += 1) {
                string temp = prefix + a->ordered_values[i];
                guesses.emplace_back(temp);
                total_guesses += 1;
            }
        } else {
            // 计算每个线程的负载
            int chunk_size = total_values / NUM_THREADS;
            int remainder = total_values % NUM_THREADS;
            
            pthread_t threads[NUM_THREADS];
            
            // 为每个线程创建独立的参数（因为需要独立的local_results）
            // 使用动态分配确保线程安全
            struct ThreadData {
                segment* seg;
                int start;
                int end;
                vector<string> local_results;
                int local_count;
                string prefix;
            };
            
            ThreadData* thread_data[NUM_THREADS];
            
            int current_start = 0;
            
            // 创建并启动线程
            for (int t = 0; t < NUM_THREADS; t++) {
                int thread_end = current_start + chunk_size;
                if (t < remainder) {
                    thread_end += 1;
                }
                
                thread_data[t] = new ThreadData();
                thread_data[t]->seg = a;
                thread_data[t]->start = current_start;
                thread_data[t]->end = thread_end;
                thread_data[t]->local_count = 0;
                thread_data[t]->prefix = prefix;
                
                // 使用lambda表达式作为线程函数（需要C++11）
                // 为了兼容性，定义一个简单的wrapper函数
                // 这里我们使用一个辅助函数
                
                current_start = thread_end;
            }
            
            // 定义线程函数
            auto thread_worker = [](void* arg) -> void* {
                ThreadData* data = (ThreadData*)arg;
                for (int i = data->start; i < data->end; i++) {
                    string temp = data->prefix + data->seg->ordered_values[i];
                    data->local_results.emplace_back(temp);
                    data->local_count++;
                }
                return NULL;
            };
            
            // 创建线程
            for (int t = 0; t < NUM_THREADS; t++) {
                pthread_create(&threads[t], NULL, 
                    [](void* arg) -> void* {
                        ThreadData* data = (ThreadData*)arg;
                        for (int i = data->start; i < data->end; i++) {
                            string temp = data->prefix + data->seg->ordered_values[i];
                            data->local_results.emplace_back(temp);
                            data->local_count++;
                        }
                        return NULL;
                    }, 
                    (void*)thread_data[t]);
            }
            
            // 等待线程完成并合并结果
            for (int t = 0; t < NUM_THREADS; t++) {
                pthread_join(threads[t], NULL);
                
                for (const string& g : thread_data[t]->local_results) {
                    guesses.emplace_back(g);
                }
                total_guesses += thread_data[t]->local_count;
                
                delete thread_data[t];
            }
        }
    }
}