#include "search.h"
#include "time_manager.h"
#include <iostream>

#include <iostream>

namespace Prometheus {
namespace Search {

ThreadPool Threads;

void ThreadPool::init(int count) {
  stop();
  // delete old
  for (auto w : workers)
    delete w;
  workers.clear();

  for (int i = 0; i < count; ++i) {
    workers.push_back(new SearchWorker(i));
  }
}

void ThreadPool::set_thread_count(int count) {
  if (count < 1)
    count = 1;
  if (workers.size() == (size_t)count)
    return;
  init(count);
}

void ThreadPool::start_search(const Board &board, const SearchLimits &limits) {
  stop(); // Stop any running

  // Initialize Time Manager (once, global)
  Timer.init(limits, board.side_to_move());

  SearchLimits limits_copy = limits;
  limits_copy.multipv = default_multipv;
  limits_copy.contempt = default_contempt;

  if (workers.size() == 1) {
    SearchWorker *worker = workers[0];
    worker->rootBoard = board;
    worker->limits = limits_copy;
    worker->should_stop = false;
    worker->searching = true;

    // Run on main thread (bypassing worker thread stack limits)
    worker->iter_deep();
    worker->searching = false;
    return;
  }

  for (auto &worker : workers) {
    worker->rootBoard = board;
    worker->limits = limits_copy;
    // worker->nodes = 0;
    worker->should_stop = false;
    worker->searching = true;

    // Signal start
    std::unique_lock<std::mutex> lock(worker->mutex);
    worker->start_flag = true;
    worker->cv.notify_one();
  }

  // Wait for main thread to finish?
  // Usually UCI calls start and then waits or loops?
  // Standard UCI: 'go' is non-blocking usually? No, "The engine should send
  // 'bestmove' ... when it is ready". If 'go infinite', we trigger threads and
  // return. If 'go depth X', we trigger threads and return. BUT, the engine
  // must not exit UCI loop. So 'go' command usually spawns a thread or lets the
  // thread pool handle it. Here, main thread (worker 0) does the work? If we
  // block in `go`, we can't receive `stop`. So `go` should be non-blocking. But
  // `uci.cpp` calls `iter_deep` which was blocking. We should make `uci.cpp`
  // wait for `stop` or `ponderhit`. Actually, `UCI::loop` reads standard input.
  // `go` parses and starts search. If `go` blocks, we can't read 'stop'. So
  // `go` MUST be non-blocking.

  // Current design: Search threads run. Main thread IS a search thread?
  // Often, thread 0 is special and does PV printing.
  // But `UCI::loop` needs to remain responsive.
  // So `SearchWorker` 0 should also be a separate thread from `UCI::loop`
  // thread. My `init` creates `count` threads. So even worker 0 is a thread. So
  // `start` just notifies them.
}

void ThreadPool::stop() {
  for (auto w : workers) {
    w->should_stop = true;
  }
  // Wait for searching to become false? or join?
  // We don't join threads, they stay alive waiting for next search.
  // But we should wait for them to finish current search iteration.
  for (auto w : workers) {
    while (w->searching) {
      std::this_thread::yield();
    }
  }
}

SearchWorker::SearchWorker(int id) : thread_id(id) {
  searching = false;
  should_stop = false;
  moveLists.resize(128); // Max ply 128
  thread = std::thread(&SearchWorker::search, this);
}

SearchWorker::~SearchWorker() {
  stop();
  {
    std::unique_lock<std::mutex> lock(mutex);
    exit_thread = true;
    start_flag = true; // wake up
    cv.notify_all();
  }
  if (thread.joinable())
    thread.join();
}

void SearchWorker::stop() { should_stop = true; }

void SearchWorker::search() {
  while (true) {
    // Wait for start
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return start_flag; });

    if (exit_thread)
      return;

    start_flag = false;
    lock.unlock();

    searching = true;

    // Check Book (Thread 0 only)
    if (thread_id == 0 && Threads.use_book) {
      Move m = BookInstance.probe(rootBoard, true); // Pick best move
      if (m != Move::NONE) {
        std::cout << "bestmove " << m.to_uci() << std::endl;
        searching = false;
        continue;
      }
    }

    iter_deep();
    searching = false;
  }
}

} // namespace Search
} // namespace Prometheus
