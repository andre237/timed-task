# timed-task
C++ timer implementation to rhythmically execute a given task

Easy to use, header only, timer. Just inherit the TimerTask class into your own class and define what the timer should do. Here's an example: 

```c++
#include <iostream>
#include "timed-task.h"

class UpdateTask : public TimerTask {
public:
    explicit UpdateTask(uint64_t rate, TimeUnit ratio) :
        TimerTask(rate, ratio) {}

private:
    void doAction() override {   
        std::cout << "update task\n";
    }
};

int main() {
    // once instantiated, it is running (RAII-style)
    UpdateTask update(100, TimeUnit::milliseconds);
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // going out of scope immediately stops it
    // no need to worry on stopping it manually

    return 0;
}
```

The timer performs cadencing/compensation at every single execution to ensure the rhythm is kept all along. Don't worry if your `doAction()` implementation may take a while. The timer is here to balance it. See example above: 

```c++
void doAction() override {
      std::random_device rd; 
      std::mt19937 eng(rd()); // 
      std::uniform_int_distribution<> distr(1, 5);

      int t = distr(eng) * 10;
      std::this_thread::sleep_for(std::chrono::milliseconds(t));
      std::cout << "update task " << t << std::endl;
      
      // even tough this method's execution time is random
      // the timer will keep the execution cadenced
}
```
Built-in statistics are available as well, enabled by default:

```shell
Samples taken: 800
Deviation average: 0.077079 milliseconds
Compensation average: 69.131133 milliseconds
Max variance: 0.134759 milliseconds
Min variance: 0.006714 milliseconds
Tolerance exceeded 0 times
```

