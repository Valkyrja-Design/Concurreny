#include <semaphore>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <queue>
#include <memory>

int main(){
    constexpr int cap = 20;
    constexpr int cust_threads = 200;
    int customers = 0;
    int customers_done = 0;
    std::queue<std::pair<std::thread::id, std::shared_ptr<std::binary_semaphore>>> sofa{};
    std::queue<std::pair<std::thread::id, std::shared_ptr<std::binary_semaphore>>> standing{};
    std::counting_semaphore<> customer{0}, customer_done{0}, barber_done{0}, payed{0}, accepted{0};
    std::mutex count{}, reg{}, stream{};
    
    auto barber_func = [&]{
        while (true){
            customer.acquire();
            count.lock();
            auto sem = sofa.front();
            sofa.pop();
            count.unlock();

            {
                std::lock_guard<std::mutex> lock{stream};
                std::cout << "Barber " << std::this_thread::get_id() << ": cutting hair of " << sem.first << std::endl;
            }
            sem.second->release();

            barber_done.release();
            customer_done.acquire();

            payed.acquire();
            {
                std::lock_guard<std::mutex> lock{stream};
                std::cout << "Barber " << std::this_thread::get_id() << ": accepted payment" << std::endl;
            }
            accepted.release();
        }
    };

    auto customer_func = [&]{
        auto sem = std::make_shared<std::binary_semaphore>(0);
        auto tid = std::this_thread::get_id();

        count.lock();
        if (customers == cap){
            ++customers_done;
            {
                std::lock_guard<std::mutex> lock{stream};
                std::cout << "Customer " << tid << ": shop full" << std::endl;
                std::cout << "Customers done: " << customers_done << std::endl;
            }
            count.unlock();
            return;
        }
        ++customers;
        if (sofa.size() < 4 && standing.empty()){
            sofa.push({tid, sem});
            {
                std::lock_guard<std::mutex> lock{stream};
                std::cout << "Customer " << tid << ": sitting on sofa" << std::endl;
            }
            customer.release();
        } else {
            {
                std::lock_guard<std::mutex> lock{stream};
                std::cout << "Customer " << tid << ": sofa full, standing" << std::endl;
            }
            standing.push({tid, sem});
        }
        count.unlock();

        sem->acquire();
        {
            std::lock_guard<std::mutex> lock{stream};
            std::cout << "Customer " << tid << ": getting haircut" << std::endl;
        }

        customer_done.release();
        barber_done.acquire();

        reg.lock();
        {
            std::lock_guard<std::mutex> lock{stream};
            std::cout << "Customer " << tid << ": paying" << std::endl;
        }
        payed.release();
        accepted.acquire();
        reg.unlock();

        count.lock();
        --customers;
        ++customers_done;
        {
            std::lock_guard<std::mutex> lock{stream};
            std::cout << "Customer " << tid << ": leaving" << std::endl;
            std::cout << "Customers done: " << customers_done << std::endl;
        }
        if (!standing.empty()){
            sofa.push(standing.front());
            standing.pop();
            {
                std::lock_guard<std::mutex> lock{stream};
                std::cout << "Customer " << sofa.back().first << ": sitting on sofa" << std::endl;
            }
            customer.release();
        }
        count.unlock();
    };


    std::vector<std::jthread> barbers;

    for (int i=0;i<3;++i){
        barbers.emplace_back(barber_func);
    }

    std::vector<std::jthread> customer_threads;
    for (int i=0;i<cust_threads;++i){
        customer_threads.emplace_back(customer_func);
    }
}   