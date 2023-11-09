#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>
#include <iostream>

int sol_with_extra_thread(){
    std::counting_semaphore<> oxygen_queue{0}, hydrogen_queue{0}, oxygen{0}, hydrogen{0};
    std::counting_semaphore<> bonded{0}, mutex{1};
    std::mutex stream;
    auto done = 0;

    constexpr auto noxygen = 100;
    constexpr auto nhydrogen = 200;
    constexpr auto nmolecules = 100;
    auto molecules_completed = 0;

    auto oxy_func = [&]{
        oxygen.release();
        oxygen_queue.acquire();

        {
            std::lock_guard<std::mutex> lk{stream};
            std::cout << "Oxygen bonded\n";
        }

        mutex.acquire();
        ++done;
        if (done == 3)
            bonded.release();
        mutex.release();
    };

    auto hyd_func = [&]{
        hydrogen.release();
        hydrogen_queue.acquire();

        {
            std::lock_guard<std::mutex> lk{stream};
            std::cout << "Hydrogen bonded\n";
        }

        mutex.acquire();
        ++done;
        if (done == 3)
            bonded.release();
        mutex.release();
    };

    auto conductor_func = [&]{
        while (molecules_completed != nmolecules){
            oxygen.acquire();
            hydrogen.acquire();
            hydrogen.acquire();

            {
                std::lock_guard<std::mutex> lk{stream};
                std::cout << "Molecule ready for bonding\n";
            }

            oxygen_queue.release();
            hydrogen_queue.release(2);

            bonded.acquire();
            done = 0;
            ++molecules_completed;
        }
    };

    std::jthread conductor{conductor_func};
    
    std::vector<std::jthread> hydrogens;
    for (int i=0;i<nhydrogen;++i){
        hydrogens.emplace_back(hyd_func);
    }

    std::vector<std::jthread> oxygens;
    for (int i=0;i<noxygen;++i){
        oxygens.emplace_back(oxy_func);
    }   
}

int main(){

}