#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace {

//---------------------------------------------------------------------------------
/**
 * @brief
 * スピンロック同期オブジェクト
 */
class SharedSpinLock final {
public:
    //---------------------------------------------------------------------------------
    /**
     * @brief	コンストラクタ
     */
    SharedSpinLock() = default;

    //---------------------------------------------------------------------------------
    /**
     * @brief	デストラクタ
     */
    ~SharedSpinLock() = default;

    //---------------------------------------------------------------------------------
    /**
     * @brief	排他ロック ( unique_lock ) を取得する
     */
    void lock() noexcept {
        int32_t e = 0;
        while (!state_.compare_exchange_weak(e, -1)) {
            e = 0;
            _mm_pause();
        }
    }

    //---------------------------------------------------------------------------------
    /**
     * @brief	排他ロック ( unique_lock ) を開放する
     */
    void unlock() noexcept {
        state_.store(0);
    }

    //---------------------------------------------------------------------------------
    /**
     * @brief	共有ロック ( shared_lock ) を取得する
     */
    void lock_shared() noexcept {
        int32_t e = 0;
        int32_t d = e + 1;

        while (!state_.compare_exchange_weak(e, d)) {
            if (e < 0) {
                // 排他ロック中なのでリセット
                e = 0;
            }
            d = e + 1;
            _mm_pause();
        }
    }

    //---------------------------------------------------------------------------------
    /**
     * @brief	共有ロック ( shared_lock ) を開放する
     */
    void unlock_shared() noexcept {
        state_.fetch_sub(1);
    }

private:
    std::atomic<int32_t> state_{}; // 0:ロックなし / -1:排他ロック / 1～:共有ロック
};
}  // namespace

int main() {

    std::vector<int> array{};
    SharedSpinLock   spinlock{};

	// 二つのスレッドでコンテナに追加し続けながら、別の二つのスレッドで要素数をチェックし続ける
    {
		// 追加スレッド
        auto add = [&]() {
            while (true) {
                {
                    std::lock_guard<decltype(spinlock)> locker(spinlock);
                    if (array.size() < 1000) {
                        array.emplace_back(array.size());
                    } else {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
            }
        };

		// 要素数チェックスレッド
        auto check = [&]() {
            while (true) {
                {
                    std::shared_lock<decltype(spinlock)> locker(spinlock);
                    if (1000 <= array.size()) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::nanoseconds(2));
            }
        };

		// スレッド４つを同時起動
        std::thread threadCheck1(check);
        std::thread threadCheck2(check);
        std::thread threadAdd1(add);
        std::thread threadAdd2(add);

		// 終了を待つ
        threadAdd1.join();
        threadAdd2.join();
		threadCheck1.join();
        threadCheck2.join();
    }

    std::cout << "Finish" << std::endl;
}
