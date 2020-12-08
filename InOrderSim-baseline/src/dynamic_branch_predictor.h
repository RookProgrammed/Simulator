#ifndef _DBP_H_
#define _DBP_H_

#include <cstdint>

#define BHR_LEN 8
#define BHT_INDEX BHR+1
#define BHT_INDEX_MASK 0x000000ff
#define BHT_NUM 256
#define BTB_INDEX 10
#define BTB_NUM 1024
#define BTB_INDEX_MASK 0x000003ff
#define BTB_TAG_MASK 0xfffffffc
#define RAS_NUM 8

//branch op
enum branch_type {cond, jal, jalr, j, jr};
//prediction flag, 2 bit
enum bht_state {s_nt, w_nt, w_t, s_t};

//btb struct
typedef struct btb
{
	uint32_t tag;
	bool valid;
	branch_type b_type;
	uint32_t target;
}btb;

//btb return type
typedef struct btb_return
{
	bool hit;
	uint32_t target;
	branch_type b_type;
}btb_return;

//RAS struct
typedef struct ras
{
	uint32_t pointer;
	uint32_t ras_entry[RAS_NUM]
}ras;

//fetch index from pc
uint32_t get_bht_index(uint32_t pc)
{
	uint32_t bht_index;
    bht_index = BHR ^ (pc >> 2);
    bht_index = bht_index & BHT_INDEX_MASK;
    return bht_index;
}

//fetch from BHT
bool bht_pred(uint32_t BHT_index)
{
	bht_state  temp_cnt;
	temp_cnt = BHT[BHT_index];
	if (temp_cnt == s_nt || temp_cnt == w_nt)
		return false;
	else
		return true;
}

//update BHR after prediction
void update_BHR(bool taken)
{
  if (taken)
    BHR = (BHR << 1) | 1;
  else
    BHR = BHR << 1;
}

//update BHT after prediction (change state)
void update_BHT(uint32_t BHT_index, bht_state state, bool b_result)
{
  if (state == s_nt && b_result)
    BHT[BHT_index] = w_nt;
  else if (state == w_nt)
  {
    if (b_result)
      BHT[BHT_index] = w_t;
    else
      BHT[BHT_index] = s_t;
	}
  else if (state == w_t)
  {
    if (b_result)
	  BHT[BHT_index] = s_t;
	else
	  BHT[BHT_index] = w_nt;
  }
  else if (pre_state == s_t && !b_result)
    BHT[BHT_index] = w_t;
}

//miss or false
void update_BTB(uint32_t pc, uint32_t dest, branch_type b_type, bool btb_miss, bool dest_mismatch)
{
	uint32_t index;
	index = c_pc >> 2;
	index = index & BTB_INDEX_MASK;
	//miss
	if (btb_miss)
	{
		//write new entry into btb
		BTB[index].valid = 1;
		BTB[index].tag = pc & BTB_TAG_MASK;
		BTB[index].b_type = b_type;
		BTB[index].target = dest;
	}
	//false
	else if (dest_mismatch)
	{
		BTB[index].b_type = b_type;
		BTB[index].target = dest;
	}

}

btb_return btb_pred(uint32_t pc)
{
	btb_ret pred;
	uint32_t index;
	uint32_t temp_tag, pc_tag, target;
	bool hit;
	index = pc >> 2;
	index = index & BTB_INDEX_MASK;
	tag_tmp = BTB[index].tag;
	pc_tag = pc_tag & BTB_TAG_MASK;
	hit = BTB[index].valid;
	pred.b_type = BTB[index].b_type;
	target = BTB[index].target;

	if (hit == false || pc_tag != tag_tmp)//btb miss, mismatch
	{
		pred.hit = false;
		pred.target = pc + 4;
	}
	else//btb hit
	{
		pred.hit = true;
		pred.target = target;
	}
	return pred;
}

void ras_push(uint32_t target)
{
	ras.pointer++;
	ras_entry[ras.pointer] = target;
}

uint32_t ras_pop()
{
	uint32_t pop_pc;
	pop_pc = ras_entry[ras.pointer];
	ras.pointer--;
	return pop_pc;
}

uint32_t ras_jalr(uint32_t pc)
{
	uint32_t pop_pc;
	pop = pop_ras();
	ras_push(pc);
}

//BHT & BHR
extern bht_state BHT[BHT_NUM]
extern int BHR

//BTB * RAS
extern ras RAS
extern btb BTB[BTB_NUM]

void brp_init()
{	
	BHR = 0;
	bht_state tmp = w_nt;
	for (int i = 0;i < BHT_NUM;i++)
	{
		BHT[i] = tmp;
	}
    //initial all btb_tags to 0
	for (int i = 0;i < BTB_NUM;i++)
	{
		BTB[i].valid = false;
		BTB[i].b_type = 0;
		BTB[i].tag = 0;
	}
	//initial RAS to 0
	ras.pointer = 0;
	for (int i = 0;i < RAS_NUM;i++)
	{
		ras_entry[i] = 0;
	}
}

//branch return type
typedef struct b_pred_return
{
	uint32_t dest;
	bool taken;
	branch_type b_type;
}b_pred_return;

//branch recovery type
typedef struct b_recov
{
	uint32_t dest;
	uint32_t pc;
	branch_type b_type;
	bht_state bht_s;
	bool mispred;
	bool taken;
}b_recov;

//branch prediction function
b_pred_return b_pred(uint32_t pc)
{
	uint32_t bht_index;
	uint32_t dest;
	b_pred_return pred;
	btb_return btb_r;
	bool taken;
	bht_index = get_bht_index(pc);
	btb_r = btb_pred(pc);
	taken = bht_pred(bht_index);
	pred.b_type = btb_r.b_type;

	if (btb_r.hit) //btb hit
	{
		if (btb_r.b_type == j || btb_r.b_type == jal || btb_r.b_type == cond) //immdiate jump
		{
			pred.dest = btb_r.target;
		}
		else if (btb_r.b_type == jr) //return
		{
			pred.dest = ras_pop();
		}
		else //register jump
		{
			uint32_t tmp;
			tmp = pc + 4;
			pred.dest = ras_jalr(tmp);
		}
	}
	else //miss
	{
		pred.dest = btb_r.target;
	}
	if (btb_r.b_type == cond)
		pred.taken = taken;
	else
		pred.taken = true;
}

#endif