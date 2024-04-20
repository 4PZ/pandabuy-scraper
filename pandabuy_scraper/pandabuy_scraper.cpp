#pragma warning( disable : 4996)

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <format>

#define FILE_PATH ""
#define OUTPUT_PATH "poland_records.txt"
#define SCRAPE_PHRASE ""

#define VAL(str) #str

std::string get_formatted_datetime() {
    tm tm_buf;
    time_t tm;

    time(&tm);
    localtime_s(&tm_buf, &tm);

    std::string buf(260, '\0');
    strftime(&buf[0], buf.size(), "%F.%H_%M_%S", &tm_buf);
    return buf.substr(0, buf.find('\0'));
}

template <typename ... Ts>
void print(const std::string_view msg, Ts&&... args) {
    std::cout << "[pandziak] | at: " << get_formatted_datetime() << " | " << std::vformat(msg, std::make_format_args(std::forward<Ts>(args)...)) << std::endl;
}

void read_and_transfer_to_other_threads(const std::string& filename, std::vector<std::string>& lines, std::mutex& mtx, std::condition_variable& cv, bool& ready) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        print("Error opening file: " + filename);
        ready = true;
        cv.notify_all();
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::lock_guard<std::mutex> lock(mtx);
        lines.push_back(line);
    }

    file.close();
    ready = true;
    cv.notify_all();
}

void process_database_records(const std::vector<std::string>& lines, std::unordered_set<std::string>& poland_records, std::mutex& mtx) {
    for (const auto& line : lines) {
        if (line.find(SCRAPE_PHRASE) != std::string::npos) {
            std::lock_guard<std::mutex> lock(mtx);
            poland_records.insert(line);
        }
    }
}

void save_valid_reccords(const std::unordered_set<std::string>& poland_records, const std::string& output_filename, std::mutex& mtx) {
    std::ofstream output_ofstream(output_filename);
    if (!output_ofstream.is_open()) {
        print("Error opening output file: " + output_filename);
        return;
    }

    for (const auto& record : poland_records) {
        output_ofstream << record << std::endl;
    }

    output_ofstream.close();
}

int main() {
    std::vector<std::string> lines;
    std::unordered_set<std::string> poland_records;

    const int threads_i = std::thread::hardware_concurrency() - 1;
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;

    auto start = std::chrono::steady_clock::now();

    std::thread file_thread(read_and_transfer_to_other_threads, FILE_PATH, std::ref(lines), std::ref(mtx), std::ref(cv), std::ref(ready));
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&ready] { 
            return ready; }
        );
    }

    file_thread.join(); 

    std::vector<std::thread> threads;
    threads.reserve(threads_i);
    for (int i = 0; i < threads_i; ++i) {
        threads.emplace_back(process_database_records, std::cref(lines), std::ref(poland_records), std::ref(mtx));
    }

    for (auto& t : threads) {
        t.join();
    }

    for (const auto& record : poland_records) {
        print(record);
    }

    save_valid_reccords(poland_records, OUTPUT_PATH, mtx);
    print("Records from Poland saved");

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "Execution time: " << elapsed_seconds.count() << " seconds" << std::endl;

    return 0;
}