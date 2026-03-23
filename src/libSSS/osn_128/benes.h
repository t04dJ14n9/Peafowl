#pragma once

#include <vector>

#include <cryptoTools/Common/BitVector.h>

class Benes
{
	// 用于存储排列的向量
	std::vector<int> perm;
	// 用于存储排列的逆向量
	std::vector<int> inv_perm;
	// 用于存储每个开关的状态
	std::vector<std::vector<int>> switched;
	// 用于存储深度优先搜索的路径
	std::vector<char> path;
	mpz_t p;

	// 用于进行深度优先搜索的函数，参数为当前节点的索引和路由方向
	void DFS(int idx, int route);

	// 用于对Benes网络进行评估的函数，参数为网络规模，当前层级，当前排列索引和源数据
	void gen_benes_eval(int n, int lvl_p, int perm_idx, std::vector<uint64_t> &src);

public:
	// 用于将Benes网络的状态保存到文件的函数，参数为文件名
	bool dump(const std::string &filename);

	// 用于从文件加载Benes网络的状态的函数，参数为文件名
	bool load(const std::string &filename);
	void initialize(int values, int levels, std::vector<uint64_t> &p); // 用于初始化Benes网络的函数，参数为网络规模和层级数

	osuCrypto::BitVector return_switches(int N); // 用于返回Benes网络中所有开关状态的函数，参数为网络规模

	// 用于生成Benes网络的路由方案的函数，参数为网络规模，当前层级，当前排列索引，源数据和目标数据
	void gen_benes_route(int n, int lvl_p, int perm_idx, const std::vector<int> &src,
						 const std::vector<int> &dest);

	// 用于对Benes网络进行评估的函数，参数为网络规模，当前层级，当前排列索引和源数据
	void gen_benes_eval(int n, int lvl_p, int perm_idx, std::vector<oc::block> &src);

	// 用于对Benes网络进行掩码评估的函数，参数为网络规模，当前层级，当前排列索引，源数据和输出数据
	void gen_benes_masked_evaluate(int n, int lvl_p, int perm_idx, std::vector<oc::block> &src,
								   std::vector<std::vector<std::array<osuCrypto::block, 2>>> &ot_output);

	// 用于返回生成Benes网络时使用的开关状态的函数，参数为网络规模
	osuCrypto::BitVector return_gen_benes_switches(int values);
};