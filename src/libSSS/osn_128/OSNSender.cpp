#include "OSNSender.h"
#include "libOTe/Base/BaseOT.h"
#include "cryptoTools/Common/BitVector.h"
#include <cryptoTools/Crypto/AES.h>
#include <libOTe/TwoChooseOne/SilentOtExtReceiver.h>
#include <libOTe/TwoChooseOne/IknpOtExtReceiver.h>
#include "benes.h"
#include "op.h"
#include <iterator>
#include <gmp.h>
#include <iostream>
#include "cryptoTools/Network/IOService.h"

#include <string>

using namespace std;
using namespace osuCrypto;

std::vector<std::array<osuCrypto::block, 2>> OSNSender::gen_benes_server_osn(int values,
																			 std::vector<oc::Channel> &chls,
																			 int ot_type)
{
	osuCrypto::BitVector switches = benes.return_gen_benes_switches(values);
	std::vector<std::array<osuCrypto::block, 2>> recvMsg(switches.size());
	std::vector<std::array<osuCrypto::block, 2>> recvCorr(switches.size());
	Channel &chl = chls[0];

	print_intermediate_value("pass", "Sender test1.1");

	if (ot_type == 0)
	{
		std::vector<osuCrypto::block> tmpMsg(switches.size());
		osuCrypto::BitVector choices(switches.size());

		silent_ot_recv(choices, tmpMsg, chls);
		AES aes(ZeroBlock);
		for (auto i = 0; i < recvMsg.size(); i++)
		{
			recvMsg[i] = {tmpMsg[i], aes.ecbEncBlock(tmpMsg[i])};
		}
		osuCrypto::BitVector bit_correction = switches ^ choices;
		chl.send(bit_correction);
	}
	else
	{

		std::vector<osuCrypto::block> tmpMsg(switches.size());

		print_intermediate_value("pass", "Sender test1.2");

		print_intermediate_vector(chls, "chls");

		rand_ot_recv(switches, tmpMsg, chls);

		print_intermediate_value("pass", "Sender test1.3");

		AES aes(ZeroBlock);
		for (auto i = 0; i < recvMsg.size(); i++)
			recvMsg[i] = {tmpMsg[i], aes.ecbEncBlock(tmpMsg[i])};
	}
	chl.recv(recvCorr.data(), recvCorr.size());
	block temp_msg[2], temp_corr[2];
	mpz_t tmp[2];
	mpz_inits(tmp[0], tmp[1], NULL);

	print_intermediate_value("pass", "Sender test1.4");

	for (int i = 0; i < recvMsg.size(); i++)
	{
		if (switches[i] == 1)
		{
			block2mpz(recvCorr[i][0], tmp[0]);
			block2mpz(recvMsg[i][0], tmp[1]);
			mpz_sub(tmp[0], tmp[0], tmp[1]);
			mpz_mod(tmp[0], tmp[0], p);
			// gmp_printf("============%Zx\n",tmp[0]);
			temp_msg[0] = mpz2block(tmp[0]);
			// cout << temp_msg[0] << endl;

			block2mpz(recvCorr[i][1], tmp[0]);
			block2mpz(recvMsg[i][1], tmp[1]);
			mpz_sub(tmp[0], tmp[0], tmp[1]);
			mpz_mod(tmp[0], tmp[0], p);
			temp_msg[1] = mpz2block(tmp[0]);

			recvMsg[i] = {temp_msg[0], temp_msg[1]};
		}
	}

	print_intermediate_value("pass", "Sender test1.5");

	return recvMsg;
}

std::vector<std::vector<uint64_t>> OSNSender::run_osn(size_t size,
													  std::vector<int> &dest,
													  std::vector<uint64_t> &p_array,
													  int ot_type,
													  string Sip,
													  string sessionHint,
													  size_t num_threads)
{
	mpz_t p;
	mpz_init(p);
	mpz_import(p, p_array.size(), -1, sizeof(uint64_t), 0, 0, p_array.data());
	print_intermediate_value(size, "size");
	print_intermediate_value(ot_type, "ot_type");
	print_intermediate_vector(dest, "dest");
	if (allow_print_intermediate_value)
		gmp_printf("p = %Zd\n", p);

	print_intermediate_value("pass", "Sender test1");

	Session session;
	if (sessionHint == "")
		session.start(this->ios, Sip, EpMode::Server);
	else
		session.start(this->ios, Sip, EpMode::Server, sessionHint);

	vector<Channel> chls;
	for (size_t i = 0; i < num_threads; i++)
	{
		chls.emplace_back(session.addChannel());
	}

	int values = size;
	int N = int(ceil(log2(values)));
	int levels = 2 * N - 1;

	benes.initialize(values, levels, p_array);

	std::vector<int> src(values);
	for (int i = 0; i < src.size(); ++i)
		src[i] = i;

	benes.gen_benes_route(N, 0, 0, src, dest);

	print_intermediate_value("pass", "Sender test2");

	std::vector<std::array<osuCrypto::block, 2>> ot_output = gen_benes_server_osn(values, chls, p, ot_type);

	// cout << IoStream::lock;
	// cout <<"ot_output:"<< ot_output[0][1]<< ot_output[0][2] <<endl; //! test 
	// cout << IoStream::unlock;

	std::vector<block> input_vec(values);
	std::vector<std::vector<uint64_t>> input_vec_ul(values);

	print_intermediate_value("pass", "Sender test3");

	chls[0].recv(input_vec.data(), input_vec.size());

	// cout << IoStream::lock;
	// cout <<"input:"<<  input_vec[1]<< input_vec[2] <<endl; //! test 
	// cout << IoStream::unlock;

	print_intermediate_value("pass", "Sender test4");

	std::vector<std::vector<std::array<osuCrypto::block, 2>>> matrix_ot_output(
		levels, std::vector<std::array<osuCrypto::block, 2>>(values));

	print_intermediate_value("pass", "Sender test5");

	int ctr = 0;
	for (int i = 0; i < levels; ++i)
	{
		for (int j = 0; j < values / 2; ++j)
			matrix_ot_output[i][j] = ot_output[ctr++];
	}

	print_intermediate_value("pass", "Sender test6");

	benes.gen_benes_masked_evaluate(N, 0, 0, input_vec, matrix_ot_output);
	// cout <<"input:"<<  input_vec[1]<< input_vec[2] <<endl; //! test 

	print_intermediate_value("pass", "Sender test7");

	for (int i = 0; i < input_vec.size(); ++i)
	{
		for (uint64_t x : input_vec[i].as<uint64_t>())
			input_vec_ul[i].push_back(x);
	}

	for (auto &chl : chls)
	{
		this->totalDataSent += chl.getTotalDataSent();
		this->totalDataRecv += chl.getTotalDataRecv();
	}

	return input_vec_ul; // share
}

size_t OSNSender::getTotalDataSent(){
	return this->totalDataSent;
}

size_t OSNSender::getTotalDataRecv(){
	return this->totalDataRecv;
}

void OSNSender::setTimer(Timer &timer)
{
	this->timer = &timer;
}

template <typename T>
void OSNSender::print_intermediate_vector(T &value, std::string name)
{
	if (allow_print_intermediate_value)
	{
		std::cout << name << std::endl;
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			std::cout << i << " " << value[i] << std::endl;
		}
	}
}

template <typename T>
void OSNSender::print_intermediate_value(T value, std::string name)
{
	if (allow_print_intermediate_value)
	{
		std::cout << name << "=" << value << std::endl;
	}
}

void OSNSender::silent_ot_recv(osuCrypto::BitVector &choices,
							   std::vector<osuCrypto::block> &recvMsg,
							   std::vector<oc::Channel> &chls)
{
	// std::cout << "\n Silent OT receiver!!\n";
	size_t num_threads = chls.size();
	size_t total_len = choices.size();
	vector<BitVector> tmpChoices(num_threads);
	auto routine = [&](size_t tid)
	{
		size_t start_idx = total_len * tid / num_threads;
		size_t end_idx = total_len * (tid + 1) / num_threads;
		end_idx = ((end_idx <= total_len) ? end_idx : total_len);
		size_t size = end_idx - start_idx;

		osuCrypto::PRNG prng0(_mm_set_epi32(4253465, 3434565, 234435, 23987045));
		osuCrypto::u64 numOTs = size;

		osuCrypto::SilentOtExtReceiver recv;
		recv.configure(numOTs);

		tmpChoices[tid].copy(choices, start_idx, size);
		std::vector<oc::block> tmpMsg(size);
		recv.silentReceive(tmpChoices[tid], tmpMsg, prng0, chls[tid]);

		std::copy_n(tmpMsg.begin(), size, recvMsg.begin() + start_idx);
	};
	vector<thread> thrds(num_threads);
	for (size_t t = 0; t < num_threads; t++)
		thrds[t] = std::thread(routine, t);
	for (size_t t = 0; t < num_threads; t++)
		thrds[t].join();
	choices.resize(0);
	for (size_t t = 0; t < num_threads; t++)
		choices.append(tmpChoices[t]);
}

void OSNSender::rand_ot_recv(osuCrypto::BitVector &choices,
							 std::vector<osuCrypto::block> &recvMsg,
							 std::vector<oc::Channel> &chls)
{
	// std::cout << "\n Ot receiver!!\n";

	size_t num_threads = chls.size();
	size_t total_len = choices.size();
	vector<BitVector> tmpChoices(num_threads);

	print_intermediate_value("pass", "Sender test1.2.1");

	auto routine = [&](size_t tid)
	{
		size_t start_idx = total_len * tid / num_threads;
		size_t end_idx = total_len * (tid + 1) / num_threads;
		end_idx = ((end_idx <= total_len) ? end_idx : total_len);
		size_t size = end_idx - start_idx;

		osuCrypto::PRNG prng0(_mm_set_epi32(4253465, 3434565, 234435, 23987045));
		osuCrypto::u64 numOTs = size; // input.length();
		std::vector<osuCrypto::block> baseRecv(128);
		std::vector<std::array<osuCrypto::block, 2>> baseSend(128);
		osuCrypto::BitVector baseChoice(128);

		prng0.get((osuCrypto::u8 *)baseSend.data()->data(), sizeof(osuCrypto::block) * 2 * baseSend.size());

		print_intermediate_value("pass", "Sender test thread 1");

		print_intermediate_value(chls[tid], "chls[tid]");

		osuCrypto::DefaultBaseOT baseOTs;
		baseOTs.send(baseSend, prng0, chls[tid], 1);

		print_intermediate_value("pass", "Sender test thread 2");

		osuCrypto::IknpOtExtReceiver recv;
		recv.setBaseOts(baseSend);

		tmpChoices[tid].copy(choices, start_idx, size);
		std::vector<oc::block> tmpMsg(size);

		recv.receive(tmpChoices[tid], tmpMsg, prng0, chls[tid]);
		std::copy_n(tmpMsg.begin(), size, recvMsg.begin() + start_idx);

		print_intermediate_value("pass", "Sender test thread 3");
	};

	print_intermediate_value("pass", "Sender test1.2.3");
	vector<thread> thrds(num_threads);
	for (size_t t = 0; t < num_threads; t++)
		thrds[t] = std::thread(routine, t);
	for (size_t t = 0; t < num_threads; t++)
		thrds[t].join();
	choices.resize(0);
	for (size_t t = 0; t < num_threads; t++)
		choices.append(tmpChoices[t]);
}

OSNSender::OSNSender(size_t ios_threads) : ios(ios_threads)
{
	ios.showErrorMessages(false);
	this->totalDataSent = 0;
	this->totalDataRecv = 0;
}

// void OSNSender::init(size_t size, std::vector<int> &dest, std::vector<uint64_t> &p, int ot_type, string Sip, string sessionHint, size_t num_threads)
// {
// 	this->size = size;
// 	this->ot_type = ot_type;
// 	this->dest = dest;
// 	mpz_init(this->p);
// 	mpz_import(this->p, p.size(), -1, sizeof(uint64_t), 0, 0, p.data());
// 	print_intermediate_value(this->size, "size");
// 	print_intermediate_value(this->ot_type, "ot_type");
// 	print_intermediate_vector(this->dest, "dest");
// 	if (allow_print_intermediate_value)
// 		gmp_printf("p = %Zd\n", this->p);

// 	if (sessionHint == "")
// 		this->session.start(this->ios, Sip, EpMode::Server);
// 	else
// 		this->session.start(this->ios, Sip, EpMode::Server, sessionHint);

// 	for (size_t i = 0; i < num_threads; i++)
// 	{
// 		this->chls.emplace_back(this->session.addChannel());
// 	}

// 	int values = size;
// 	int N = int(ceil(log2(values)));
// 	int levels = 2 * N - 1;

// 	benes.initialize(values, levels, p);

// 	std::vector<int> src(values);
// 	for (int i = 0; i < src.size(); ++i)
// 		src[i] = i;

// 	benes.gen_benes_route(N, 0, 0, src, dest);
// }