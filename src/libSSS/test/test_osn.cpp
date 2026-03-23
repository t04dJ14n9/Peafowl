#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <gmp.h>

#include "cryptoTools/Network/IOService.h"

#include "OSNReceiver.h"
#include "OSNSender.h"
#include "op.h"

using namespace osuCrypto;
using namespace std;

// 声明全局变量
// vector<block> sender_shares;
// vector<block> receiver_shares;
// vector<block> receiver_set;
vector<vector<uint64_t>> sender_shares;
vector<vector<uint64_t>> receiver_shares;
vector<vector<uint64_t>> receiver_set;
vector<int> permutation;
string ip = "127.0.0.1:12222";
vector<uint64_t> p_array = {1ul, 1ul};

void sender(size_t size, string hint, size_t num_threads)
{
	std::vector<int> dest(size);
	for (int i = 0; i < dest.size(); ++i)
		dest[i] = i;

	// osuCrypto::PRNG prng(_mm_set_epi32(425323, 334565, 0, 235)); // we need to modify this seed

	// for (int i = size - 1; i > 0; i--)
	// {
	// 	int loc = prng.get<uint64_t>() % (i + 1);
	// 	std::swap(dest[i], dest[loc]);
	// }

	OSNSender osn(size, dest, p_array);
	// osn.init(size, dest, p_array, 1, ip, hint, num_threads);
	Timer timer;
	osn.setTimer(timer);
	timer.setTimePoint("before run_osn");

	sender_shares = osn.run_osn(size, p_array, 1, ip, hint, num_threads);
	// sender_shares = osn.run_osn(size, dest, p_array, 1, ip, "2", num_threads);
	timer.setTimePoint("after run_osn");
	permutation = dest;

	size_t sent = 0, recv = 0;
	sent = osn.getTotalDataSent();
	recv = osn.getTotalDataRecv();
	cout << IoStream::lock;
	cout << "Sender:" << endl;
	cout << timer << endl;
	cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
		 << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << endl;
	cout << IoStream::unlock;


}

void receiver(size_t size, string hint, size_t num_threads)
{
	OSNReceiver osn;
	Timer timer;
	osn.setTimer(timer);
	timer.setTimePoint("before run_osn");

	std::pair<std::vector<vector<uint64_t>>, std::vector<vector<uint64_t>>> result = osn.run_osn(size, p_array, 1, ip, hint, num_threads);
	// result = osn.run_osn(size, p_array, 1, ip, "2", num_threads);
	receiver_set = result.first;
	receiver_shares = result.second;
	timer.setTimePoint("after run_osn");

	size_t sent = 0, recv = 0;
	sent = osn.getTotalDataSent();
	recv = osn.getTotalDataRecv();
	cout << IoStream::lock;
	cout << "Recver:" << endl;
	cout << timer << endl;
	cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
		 << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << endl;
	cout << IoStream::unlock;
}

int check_result(size_t size)
{
	cout << "checking result..." << endl;
	int correct_cnt = 0;
	mpz_t tmp, a, b, c, p;
	mpz_init(tmp);
	mpz_init(a);
	mpz_init(b);
	mpz_init(c);
	mpz_init(p);
	mpz_import(p, p_array.size(), -1, sizeof(uint64_t), 0, 0, p_array.data());

	cout << "checking result...2" << endl;

	for (auto i = 0; i < size; i++)
	{
		// cout << "checking result..."<< i+3 << endl;
		mpz_import(a, sender_shares[i].size(), -1, sizeof(uint64_t), 0, 0, sender_shares[i].data());
		// cout << "checking result..."<< i+3 << "a" << endl;
		mpz_import(b, receiver_shares[i].size(), -1, sizeof(uint64_t), 0, 0, receiver_shares[i].data());
		// cout << "checking result..."<< i+3 << "b" << endl;
		mpz_import(c, receiver_set[permutation[i]].size(), -1, sizeof(uint64_t), 0, 0, receiver_set[permutation[i]].data());
		// cout << "checking result..."<< i+3 << "c" << endl;
		// gmp_printf("a = %Zd\nb = %Zd\np = %Zd\nc = %Zd\n", a, b, p, c);
		mpz_add(tmp, a, b);
		// gmp_printf("tmp = %Zd\n", tmp);
		mpz_mod(tmp, tmp, p);
		// gmp_printf("tmp = %Zd\n", tmp);
		if (0 == mpz_cmp(tmp, c))
		{
			correct_cnt++;
		}
	}
	return correct_cnt;
}

void test_network(){
	IOService ios(4);
	IOService ios2(4);

	auto ip = std::string("127.0.0.1");
    auto port = 12222;

    std::string serversIpAddress = ip + ':' + std::to_string(port);

    u64 numSession = 10;

    for (u64 i = 0; i < numSession; ++i)
    {
        // The server will create many sessions, each will find one 
        // of the clients. Optionally a sessionHint/serviceName can be 
        // provided
        Session perPartySession(ios, serversIpAddress, SessionMode::Server /* , serviceName */);
		// cout << "12" << endl;

        // On some other thread/program/computer, a client can complete the
        // session and add a channel.
        {
            Channel clientChl = Session(ios, serversIpAddress, SessionMode::Client /* , serviceName */).addChannel();
            clientChl.send(std::string("message"));
        }

        // Create a channel for this session, even before the client has connected.
        Channel serverChl = perPartySession.addChannel();

        std::string msg;
        serverChl.recv(msg);
    }
}

int main(int argc, char **argv)
{
	test_network();
	size_t size = 1 << atoi(argv[1]);
	size_t num_threads = atoi(argv[2]);
	cout << "size:" << size << " num_threads:" << num_threads << endl;

	auto sender_thrd = thread(sender,  size, "1", num_threads);
	auto receiver_thrd = thread(receiver,size, "1", num_threads);
	sender_thrd.join();
	receiver_thrd.join();
	if (size == check_result(size))
		cout << "Correct!" << endl;
	else
		cout << "Wrong!" << endl;

	return 0;
}
