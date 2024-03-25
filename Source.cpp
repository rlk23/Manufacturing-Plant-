// Name: Rami-Lionel Kuttab


#include <mutex>
#include <list>
#include <iostream>
#include <string>
#include <random>
#include <vector>
#include <ctime>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <numeric> 


using namespace std;

const int MaxTimePart{ 1800 }, MaxTimeProduct{ 2000 };
const string log_path = "./log.txt";

mutex buffer_mutex;
mutex log_mutex;
mutex total_complete_product_mutex;
mutex m1;

ostringstream logger;
ofstream txt("log.txt");


using Us = std::chrono::microseconds;
int reference[5] = { 5,5,4,3,3 };
const int productionTimes[] = { 50, 50, 60, 60, 70 };
int buffer[5] = { 0,0,0,0,0 };
auto current_time_start = std::chrono::system_clock::now();


condition_variable partWorker;
condition_variable productWorker;

int finished_products = 0;

void PartWorker(int thread_num) {
	int num_iteration = 1;

	int unloaded[5] = { 0 };
	int remainingTimePart = MaxTimePart;

	while (num_iteration <= 2) {

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(0, 4);

		remainingTimePart = MaxTimePart;

		int load_order[5] = { 0 };

		bool hasUnloaded = false;
		for (int i = 0; i < 5; i++) {
			if (unloaded[i] > 0) {
				hasUnloaded = true;
			}
		}

		if (hasUnloaded) {
			for (int i = 0; i < 5; i++) {
				load_order[i] += unloaded[i];
				unloaded[i] = 0;
			}

			for (int i = 0; i < 5; i++) {
				load_order[dis(gen)] += 1;
			}

		}
		else {

			for (int i = 0; i < 5; i++) {
				load_order[dis(gen)] += 1;
			}
		}

		auto startend = chrono::system_clock::now();
		auto start_log = chrono::duration_cast<chrono::microseconds>(startend - current_time_start);
		unique_lock<mutex> lock1(m1);
		txt << "Current Time: " << start_log.count() << "us" << "\n";
		txt << "Iteration: " << num_iteration << "\n";
		txt << "Part Worker ID: " << thread_num << "\n";
		txt << "Status: New Load Order " << "\n";
		txt << "Accumulated Wait Time: 0us" << "\n";
		txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
		txt << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")\n";


		// manufacture parts
		this_thread::sleep_for(Us(load_order[0] * productionTimes[0] + load_order[1] * productionTimes[1] + load_order[2] * productionTimes[2] + load_order[3] * productionTimes[3] + load_order[4] * productionTimes[4]));

		int sum = 0;
		int buffer_limit[5] = { 5,5,4,3,3 };
		int current_sum[5] = { 0,0,0,0,0 };
		unique_lock<std::mutex> ulock(buffer_mutex);
		if (buffer[0] < 5 || buffer[1] < 5 || buffer[2] < 4 || buffer[3] < 3 || buffer[4] < 3) {
			for (int i = 0; i < 5; i++) {
				if (load_order[i] + buffer[i] <= buffer_limit[i]) {
					buffer[i] += load_order[i];
					sum += load_order[i];
					current_sum[i] = load_order[i];
					load_order[i] = 0;
				}
				else {
					int diff = buffer_limit[i] - buffer[i];
					if (diff < 0) {
						continue;
					}
					buffer[i] += diff;
					current_sum[i] = diff;
					load_order[i] -= diff;
					sum += diff;
				}
			}
			txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
			txt << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")" << "\n\n";
			cout << logger.str() << "\n";

			productWorker.notify_one();
			this_thread::sleep_for(Us(current_sum[0] * 20 + current_sum[1] * 20 + current_sum[2] * 30 + current_sum[3] * 30 + current_sum[4] * 40));

			if (sum != 5) {
				auto start = chrono::system_clock::now();
				if (partWorker.wait_until(ulock, start + Us(remainingTimePart), [load_order] {
					for (int i = 0; i < 5; i++) {
						if (buffer[i] + load_order[i] > reference[i]) { return true; }
					}
					return false;
					})) {
					auto end = chrono::system_clock::now();
					auto difference = chrono::duration_cast<chrono::microseconds>(end - start);
					auto now_logs = chrono::duration_cast<chrono::microseconds>(end - current_time_start);
					int waited_time = static_cast<int>(difference.count());
					remainingTimePart -= waited_time;
					if (remainingTimePart < 0) {
						remainingTimePart = 0;
					}

					txt << "Current Time: " << now_logs.count() << "us" << "\n";
					txt << "Iteration: " << num_iteration << "\n";
					txt << "Part Worker ID: " << thread_num << "\n";
					txt << "Status: Wakeup-Timeout" << "\n";
					txt << "Accumulated Waiting Time: " << difference.count() << "\n";
					txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
					txt << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")" << "\n";

					bool bufferNeedsUpdate = false;
					for (int i = 0; i < 5; i++) {
						if (reference[i] - buffer[i] < load_order[i]) {
							bufferNeedsUpdate = true;
							break;
						}
					}
					if (bufferNeedsUpdate) {

						//productWorker.notify_one();
						this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
						for (int i = 0; i < 5; i++) { unloaded[i] = load_order[i]; }
					}
					else {

						for (int i = 0; i < 5; i++) {
							if (buffer[i] + load_order[i] <= buffer_limit[i]) {
								buffer[i] += load_order[i];
								load_order[i] = 0;
							}
						}
						productWorker.notify_one();
						this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
					}

					num_iteration++;
					txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
					txt << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")\n\n";
					cout << logger.str();
					cout << '\n';
				}
				else {
					remainingTimePart = MaxTimePart;

					auto end = chrono::system_clock::now();
					auto difference = chrono::duration_cast<chrono::microseconds>(end - start);
					auto now_logs = chrono::duration_cast<chrono::microseconds>(end - current_time_start);
					txt << "Current Time: " << now_logs.count() << "\n";
					txt << "Iteration: " << num_iteration << "\n";
					txt << "Part Worker ID: " << thread_num << "\n";
					txt << "Status: Wakeup-Notified" << "\n";
					txt << "Accumulated Waiting Time: " << difference.count() << "\n";
					txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
					txt << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")" << "\n";

					bool bufferNeedsUpdate = false;
					for (int i = 0; i < 5; i++) {
						if (reference[i] - buffer[i] < load_order[i]) {
							bufferNeedsUpdate = true;
							break;
						}
					}
					if (bufferNeedsUpdate) {

						//productWorker.notify_one();
						this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
						for (int i = 0; i < 5; i++) { unloaded[i] = load_order[i]; }
					}
					else {

						for (int i = 0; i < 5; i++) {
							if (buffer[i] + load_order[i] <= buffer_limit[i]) {
								buffer[i] += load_order[i];
								load_order[i] = 0;
							}
						}
						productWorker.notify_one();
						this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
					}

					num_iteration++;
					txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
					txt << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")\n\n";
					//cout << txt.str();
					cout << '\n';

				}
			}
			else {
				txt << '\n';
				num_iteration++;
				productWorker.notify_one();
			}
		}
		else { // full buffer

			txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
			txt << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")\n\n";
			//cout << txt.str() << '\n';

			auto start = chrono::system_clock::now();
			if (partWorker.wait_until(ulock, start + Us(MaxTimePart), [load_order] {
				for (int i = 0; i < 5; i++) {
					if (reference[i] - buffer[i] < load_order[i]) { return false; }
				}
				return true; })) {
				auto end = chrono::system_clock::now();
				auto difference = chrono::duration_cast<chrono::microseconds>(end - start);
				auto now_logs = chrono::duration_cast<chrono::microseconds>(end - current_time_start);
				int waited_time = static_cast<int>(difference.count());
				remainingTimePart -= waited_time;
				if (remainingTimePart < 0) {
					remainingTimePart = 0;
				}

				txt << "Current Time: " << now_logs.count() << "us" << "\n";
				txt << "Iteration: " << num_iteration << "\n";
				txt << "Part Worker ID: " << thread_num << "\n";
				txt << "Status: Wakeup-Timeout" << "\n";
				txt << "Accumulated Waiting Time: " << difference.count() << "us" << "\n";
				txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
				txt << "Load State: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")" << "\n";
				bool bufferNeedsUpdate = false;
				for (int i = 0; i < 5; i++) {
					if (reference[i] - buffer[i] < load_order[i]) {
						bufferNeedsUpdate = true;
						break;
					}
				}
				if (bufferNeedsUpdate) {

					//productWorker.notify_one();
					this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
					for (int i = 0; i < 5; i++) { unloaded[i] = load_order[i]; }
				}
				else {

					for (int i = 0; i < 5; i++) {
						if (buffer[i] + load_order[i] <= buffer_limit[i]) {
							buffer[i] += load_order[i];
							current_sum[i] = load_order[i];
							load_order[i] = 0;
						}
					}
					productWorker.notify_one();
					this_thread::sleep_for(Us(current_sum[0] * 20 + current_sum[1] * 20 + current_sum[2] * 30 + current_sum[3] * 30 + current_sum[4] * 40));
				}

				num_iteration++;
				txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
				txt << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")\n\n";
				//std::cout << logger.str();
				cout << '\n';
			}
			else {
				// was waken up during the waiting
				remainingTimePart = MaxTimePart;
				auto end = chrono::system_clock::now();
				auto diff = chrono::duration_cast<chrono::microseconds>(end - start);
				auto now_log = chrono::duration_cast<chrono::microseconds>(end - current_time_start);
				txt << "Current Time: " << now_log.count() << "us" << "\n";
				txt << "Iteration: " << num_iteration << "\n";
				txt << "Part Worker ID: " << thread_num << "\n";
				txt << "Status: Wakeup-Notified" << "\n";
				txt << "Accumulated Waiting Time: " << diff.count() << "us" << "\n";
				txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
				txt << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")" << "\n";

				bool bufferNeedsUpdate = false;
				for (int i = 0; i < 5; i++) {
					if (reference[i] - buffer[i] < load_order[i]) {
						bufferNeedsUpdate = true;
						break;
					}
				}
				if (bufferNeedsUpdate) {

					//productWorker.notify_one();
					this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
					for (int i = 0; i < 5; i++) { unloaded[i] = load_order[i]; }
				}
				else {

					for (int i = 0; i < 5; i++) {
						if (buffer[i] + load_order[i] <= buffer_limit[i]) {
							buffer[i] += load_order[i];
							load_order[i] = 0;
						}
					}
					productWorker.notify_one();
					this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 20 + load_order[2] * 30 + load_order[3] * 30 + load_order[4] * 40));
				}
				num_iteration++;
				txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
				txt << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ", " << load_order[4] << ")\n\n";
				//std::cout << logger.str();
				std::cout << '\n';

			}
		}
	}
}

void ProductWorker(int thread_num) {


	int local_state[5]{ 0, 0, 0, 0, 0 };
	int remainingTimeProduct = 0;
	

	int num_iteration = 1;
	while (num_iteration <= 5) {

		remainingTimeProduct = 0;
		random_device rd;
		mt19937 gen(rd());
		uniform_int_distribution<int> no_type(2, 3);
		uniform_int_distribution<int> parts(0, 4);
		int pickup_order[5]{ 0, 0, 0, 0, 0 };

		bool positive_found{ false };
		for (int i = 0; i < 5; i++) {
			if (local_state[i] > 0) {
				positive_found = true;
				break;
			}
		}

		if (positive_found) {
			std::vector<int> positive_idxs;
			for (int i = 0; i < 5; i++) {
				if (local_state[i] > 0) {
					positive_idxs.push_back(i);
				}
			}

			if (positive_idxs.size() == 1) {
				std::uniform_int_distribution<int> selection_dist(2, 3);
				int selected_num = selection_dist(gen);
				while (positive_idxs.size() < selected_num) {
					int new_index = parts(gen);
					if (std::find(positive_idxs.begin(), positive_idxs.end(), new_index) == positive_idxs.end()) {
						positive_idxs.push_back(new_index);
					}
				}
			}

			int sum_state = accumulate(local_state, local_state + 5, 0);
			int remaining_elements = 5 - sum_state;

			for (int idx : positive_idxs) {
				pickup_order[idx] += local_state[idx];
				if (remaining_elements > 0) {
					pickup_order[idx]++;
					remaining_elements--;
				}
			}

			for (int idx : positive_idxs) {
				pickup_order[idx] -= local_state[idx];
			}

			std::fill_n(local_state, 5, 0);
		}
		else {
			int exclude_count = no_type(gen);
			std::vector<int> excluded_idxs;
			bool excluded_items[5] = { false };

			for (int i = 0; i < exclude_count; i++) {
				int rand_idx = parts(gen);
				if (!excluded_items[rand_idx]) {
					excluded_items[rand_idx] = true;
					excluded_idxs.push_back(rand_idx);
				}
			}

			int available_parts = 5;
			for (size_t i = 0; i < excluded_idxs.size(); i++) {
				if (i == excluded_idxs.size() - 1) {
					pickup_order[excluded_idxs[i]] = available_parts;
				}
				else {
					int maxPartsForCurrent = available_parts - (excluded_idxs.size() - i - 1);
					std::uniform_int_distribution<int> allocation_dist(1, maxPartsForCurrent);
					int parts_for_current = allocation_dist(gen);
					pickup_order[excluded_idxs[i]] = parts_for_current;
					available_parts -= parts_for_current;
				}
			}
		}

		int sum = 0;
		for (int i = 0; i < 5; i++) {
			sum += pickup_order[i];
		}
		int pickup_num[5] = { 0 }; 
		int cart_state[5] = { 0 };
		auto startend = chrono::system_clock::now();
		auto start_log = chrono::duration_cast<std::chrono::microseconds>(startend - current_time_start);
		unique_lock<mutex> lock1(m1);
		txt << "Current Time: " << start_log.count() << "us\n";
		txt << "Product Worker ID: " << thread_num << "\n";
		txt << "Iteration: " << num_iteration << "\n";
		txt << "Status: New Pickup Order\n";
		txt << "Accumulated Waiting Time: 0us\n";
		txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")\n";
		txt << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")\n";
		txt << "Local State (" << local_state[0] << ", " << local_state[1] << ", " << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")\n";
		txt << "Cart State (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")\n";

	
		unique_lock<mutex> ulock1(buffer_mutex);
		unique_lock<mutex> ulock2(total_complete_product_mutex);

		if (buffer[0] > 0 || buffer[1] > 0 || buffer[2] > 0 || buffer[3] > 0 || buffer[4] > 0) {
			for (int i = 0; i < 5; i++) {
			
				if (buffer[i] == 0) { continue; }
				if (buffer[i] - pickup_order[i] >= 0) {
					pickup_num[i] = pickup_order[i];
					cart_state[i] = pickup_order[i];
					buffer[i] -= pickup_order[i];
					sum -= pickup_order[i];
					pickup_order[i] = 0;
				}
				else {
					pickup_num[i] = buffer[i];
					cart_state[i] = buffer[i];
					pickup_order[i] -= buffer[i];
					sum -= buffer[i];
					buffer[i] = 0;
				}

			}
			txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
			txt << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
			txt << "Updated Local State: (" << local_state[0] << ", " << local_state[1] << ", " << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
			txt << "Updated Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
			txt << "Total Completed Product: " << finished_products << "\n\n";
			//cout << logger.str();
			cout << '\n';
			this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 20 + pickup_order[2] * 30 + pickup_order[3] * 30 + pickup_order[4] * 40));


		
			if (sum != 0) {
				auto start = chrono::system_clock::now();
				if (productWorker.wait_until(ulock1, start + Us(remainingTimeProduct),
					[pickup_order] {
						for (int i = 0; i < 5; i++) {
							if (buffer[i] < pickup_order[i]) {
								return true;
							}
						}
						return false;
					}))
				{
					//timeout
					auto end = chrono::system_clock::now();
					auto difference = chrono::duration_cast<chrono::microseconds>(end - start);
					auto now_logs = chrono::duration_cast<chrono::microseconds>(end - current_time_start);
					int waited_time = static_cast<int>(difference.count());
					remainingTimeProduct -= waited_time;
					if (remainingTimeProduct < 0) {
						remainingTimeProduct = 0;
					}

					txt << "Current Time: " << now_logs.count() << "us" << "\n";
					txt << "Iteration: " << num_iteration << "\n";
					txt << "Product Worker ID: " << thread_num << "\n";
					txt << "Status: " << "Wake-Timeout" << "\n";
					txt << "Accumulated Waiting Time: " << difference.count() << "us" << "\n";
					txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
					txt << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
					txt << "Local State: (" << local_state[0] << ", " << local_state[1] << ", " << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
					txt << "Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
					bool sufficient_buffer = true;  

					for (int i = 0; i < 5; i++) {
						if (buffer[i] < pickup_order[i]) {
							sufficient_buffer = false;
							break;
						}
					}
					if (sufficient_buffer) {
						
						for (int i = 0; i < 5; i++) {
							if (buffer[i] == 0) continue;
							buffer[i] -= pickup_order[i];
							pickup_num[i] += pickup_order[i];
							cart_state[i] += pickup_order[i];
							pickup_order[i] = 0;
						}
						finished_products++;
						partWorker.notify_one();
						this_thread::sleep_for(std::chrono::microseconds((pickup_num[0] * 60) + (pickup_num[1] * 60) + (pickup_num[2] * 70) + (pickup_num[3] * 70) + (pickup_num[4] * 80)));
					}
					else {
						
						partWorker.notify_one();
						this_thread::sleep_for(Us((pickup_order[0] * 20) + (pickup_order[1] * 20) + (pickup_order[2] * 30) + (pickup_order[3] * 30) + (pickup_order[4] * 40)));
						for (int i = 0; i < 5; i++) {
							local_state[i] += pickup_num[i];  
						}
					}
					num_iteration++;
					txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
					txt << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
					txt << "Updated local State: (" << local_state[0] << ", " << local_state[1] << ', ' << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
					txt << "Updated Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
					txt << "Total Completed Product: " << finished_products << "\n\n";
					//cout << logger.str();
					cout << "\n";

				}
				else {
			
					remainingTimeProduct = MaxTimeProduct;
					auto end = std::chrono::system_clock::now();
					auto difference = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
					auto now_logs = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
					txt << "Current Time: " << now_logs.count() << "us" << "\n";
					txt << "Iteration: " << num_iteration << "\n";
					txt << "Product Worker ID: " << thread_num << "\n";
					txt << "Status: Wakeup-notified" << "\n";
					txt << "Accumulated Waiting Time: " << difference.count() << "us" << "\n";
					txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
					txt << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ', ' << pickup_order[4] << ")" << "\n";
					txt << "Local State: (" << local_state[0] << ", " << local_state[1] << ", " << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
					txt << "Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
					bool sufficient_buffer = true;  

					for (int i = 0; i < 5; i++) {
						if (buffer[i] < pickup_order[i]) {
							sufficient_buffer = false;
							break;
						}
					}
					if (sufficient_buffer) {
						
						for (int i = 0; i < 5; i++) {
							if (buffer[i] == 0) continue;
							buffer[i] -= pickup_order[i];
							pickup_num[i] += pickup_order[i];
							cart_state[i] += pickup_order[i];
							pickup_order[i] = 0;
						}
						finished_products++;
						partWorker.notify_one();
						this_thread::sleep_for(std::chrono::microseconds((pickup_num[0] * 60) + (pickup_num[1] * 60) + (pickup_num[2] * 70) + (pickup_num[3] * 70) + (pickup_num[4] * 80)));
					}
					else {
					
						partWorker.notify_one();
						this_thread::sleep_for(Us((pickup_order[0] * 20) + (pickup_order[1] * 20) + (pickup_order[2] * 30) + (pickup_order[3] * 30) + (pickup_order[4] * 40)));
						for (int i = 0; i < 5; i++) {
							local_state[i] += pickup_num[i];  
						}
					}
					num_iteration++;
					txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
					txt << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
					txt << "Updated local State: (" << local_state[0] << ", " << local_state[1] << ', ' << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
					txt << "Updated Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
					txt << "Total Completed Product: " << finished_products << "\n\n";
					//std::cout << logger.str();
					std::cout << '\n';

				}

			}
			else {
			
				txt << '\n';
				num_iteration++;
				finished_products++;
				partWorker.notify_one();
			}

		}
	
		else {
			txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
			txt << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
			txt << "Updated local State: (" << local_state[0] << ", " << local_state[1] << ', ' << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
			txt << "Updated Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
			txt << "Total Completed Product: " << finished_products << "\n\n";
			//std::cout << logger.str();
			std::cout << '\n';
			auto start = chrono::system_clock::now();
			if (productWorker.wait_until(ulock1, start + Us(remainingTimeProduct),
				[pickup_order] {
					for (int i = 0; i < 5; i++) {
						if (buffer[i] < pickup_order[i]) {
							return true;
						}
					}
					return false;
				}))
			{
				//timeout
				auto end = chrono::system_clock::now();
				auto difference = chrono::duration_cast<chrono::microseconds>(end - start);
				auto now_logs = chrono::duration_cast<chrono::microseconds>(end - current_time_start);
				int waited_time = static_cast<int>(difference.count());
				remainingTimeProduct -= waited_time;
				if (remainingTimeProduct < 0) {
					remainingTimeProduct = 0;
				}
				txt << "Current Time: " << now_logs.count() << "us" << "\n";
				txt << "Iteration: " << num_iteration << "\n";
				txt << "Product Worker ID: " << thread_num << "\n";
				txt << "Status: Wakeup-notified" << "\n";
				//logger << "Problem here" << "\n";
				txt << "Accumulated Waiting Time: " << difference.count() << "us" << "\n";
				txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
				txt << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ', ' << pickup_order[4] << ")" << "\n";
				txt << "Local State: (" << local_state[0] << ", " << local_state[1] << ", " << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
				txt << "Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";

		
				bool sufficient_buffer = true;

				for (int i = 0; i < 5; i++) {
					if (buffer[i] < pickup_order[i]) {
						sufficient_buffer = false;
						break;
					}
				}
				if (sufficient_buffer) {

					for (int i = 0; i < 5; i++) {
						if (buffer[i] == 0) continue;
						buffer[i] -= pickup_order[i];
						pickup_num[i] += pickup_order[i];
						cart_state[i] += pickup_order[i];
						pickup_order[i] = 0;
					}
					finished_products++;
					partWorker.notify_one();
					this_thread::sleep_for(std::chrono::microseconds((pickup_num[0] * 60) + (pickup_num[1] * 60) + (pickup_num[2] * 70) + (pickup_num[3] * 70) + (pickup_num[4] * 80)));
				}
				else {

					partWorker.notify_one();
					this_thread::sleep_for(Us((pickup_order[0] * 20) + (pickup_order[1] * 20) + (pickup_order[2] * 30) + (pickup_order[3] * 30) + (pickup_order[4] * 40)));
					for (int i = 0; i < 5; i++) {
						local_state[i] += pickup_num[i];
					}
				}
				num_iteration++;
				txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
				txt << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
				txt << "Updated local State: (" << local_state[0] << ", " << local_state[1] << ', ' << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
				txt << "Updated Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
				txt << "Total Completed Product: " << finished_products << "\n\n";
				//std::cout << logger.str();
				std::cout << '\n';

			}
			else {

				remainingTimeProduct = MaxTimeProduct;
				auto end = chrono::system_clock::now();
				auto difference = chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_logs = chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				txt << "Current Time: " << now_logs.count() << "us" << "\n";
				txt << "Iteration: " << num_iteration << "\n";
				txt << "Product Worker ID: " << thread_num << "\n";
				txt << "Status: Wakeup-notified" << "\n";
				txt << "Accumulated Waiting Time: " << difference.count() << "us" << "\n";
				txt << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
				txt << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ', ' << pickup_order[4] << ")" << "\n";
				txt << "Local State: (" << local_state[0] << ", " << local_state[1] << ", " << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
				txt << "Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
				bool sufficient_buffer = true;

				for (int i = 0; i < 5; i++) {
					if (buffer[i] < pickup_order[i]) {
						sufficient_buffer = false;
						break;
					}
				}
				if (sufficient_buffer) {

					for (int i = 0; i < 5; i++) {
						if (buffer[i] == 0) continue;
						buffer[i] -= pickup_order[i];
						pickup_num[i] += pickup_order[i];
						cart_state[i] += pickup_order[i];
						pickup_order[i] = 0;
					}
					finished_products++;
					partWorker.notify_one();
					this_thread::sleep_for(std::chrono::microseconds((pickup_num[0] * 60) + (pickup_num[1] * 60) + (pickup_num[2] * 70) + (pickup_num[3] * 70) + (pickup_num[4] * 80)));
				}
				else {

					partWorker.notify_one();
					this_thread::sleep_for(Us((pickup_order[0] * 20) + (pickup_order[1] * 20) + (pickup_order[2] * 30) + (pickup_order[3] * 30) + (pickup_order[4] * 40)));
					for (int i = 0; i < 5; i++) {
						local_state[i] += pickup_num[i];
					}
				}

				num_iteration++;
				txt << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ", " << buffer[4] << ")" << "\n";
				txt << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ", " << pickup_order[4] << ")" << "\n";
				txt << "Updated local State: (" << local_state[0] << ", " << local_state[1] << ', ' << local_state[2] << ", " << local_state[3] << ", " << local_state[4] << ")" << "\n";
				txt << "Updated Cart State: (" << cart_state[0] << ", " << cart_state[1] << ", " << cart_state[2] << ", " << cart_state[3] << ", " << cart_state[4] << ")" << "\n";
				txt << "Total Completed Product: " << finished_products << "\n\n";
				//std::cout << logger.str();
				std::cout << '\n';
			
			}
		}
	}
}


int main() {

	const int m = 20, n = 16;


	vector<thread> PartW, ProductW;

	for (int i = 0; i < m; ++i) {
		PartW.emplace_back(PartWorker, i + 1);

	}
	for (int i = 0; i < n; ++i) {
		ProductW.emplace_back(ProductWorker, i + 1);
	}

	for (auto& i : PartW) i.join();
	for (auto& i : ProductW) i.join();

	cout << "Finish!" << endl;

	txt << "Finish!" << endl;

	return 0;
}
