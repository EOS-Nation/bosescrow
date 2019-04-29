#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <optional>

using namespace eosio;
using namespace std;

struct [[eosio::table("escrows"), eosio::contract("escrow")]] escrow_info {
    uint64_t        key;
    name            sender;
    name            receiver;
    name            auditor;
    vector<name>    approvals;
    extended_asset  ext_asset;
    string          memo;
    time_point_sec  expires;
    uint64_t        external_reference;
    bool            locked = false;

    uint64_t        primary_key() const { return key; }
    uint64_t        by_external_ref() const { return external_reference; }

    uint64_t        by_sender() const { return sender.value; }
};

typedef multi_index<"escrows"_n, escrow_info,
        indexed_by<"bysender"_n, const_mem_fun<escrow_info, uint64_t, &escrow_info::by_sender> >,
indexed_by<"byextref"_n, const_mem_fun<escrow_info, uint64_t, &escrow_info::by_external_ref> >
> escrows_table;
