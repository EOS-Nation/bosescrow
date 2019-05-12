#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/transaction.hpp>

#include <string>
#include <optional>

using eosio::const_mem_fun;
using eosio::indexed_by;
using eosio::multi_index;
using eosio::extended_asset;
using eosio::check;
using eosio::datastream;
using eosio::contract;
using eosio::print;
using eosio::name;
using eosio::asset;
using eosio::symbol;
using eosio::time_point_sec;
using eosio::current_time_point;
using std::vector;
using std::function;
using std::string;

namespace bos {
    class escrow : public contract {

    public:

        escrow(name s, name code, datastream<const char *> ds)
                : contract(s, code, ds),
                  escrows(_self, _self.value) {
            sending_code = name{code};
        }

        ~escrow();

        /**
         * Escrow contract
         */

        ACTION init(name sender, name receiver, name approver, time_point_sec expires, string memo, std::optional<uint64_t> ext_reference);

        [[eosio::on_notify("eosio.token::transfer")]]
        void transfer(name from, name to, asset quantity, string memo);

        ACTION approve(uint64_t key, name approver);

        ACTION unapprove(uint64_t key, name unapprover);

        ACTION claim(uint64_t key);

        ACTION refund(uint64_t key);

        ACTION cancel(uint64_t key);

        ACTION extend(uint64_t key, time_point_sec expires);

        ACTION close(uint64_t key);

        ACTION lock(uint64_t key, bool locked);

        // Actions using the external reference key

        ACTION approveext(uint64_t ext_key, name approver);

        ACTION unapproveext(uint64_t ext_key, name unapprover);

        ACTION claimext(uint64_t ext_key);

        ACTION refundext(uint64_t ext_key);

        ACTION cancelext(uint64_t ext_key);

        ACTION extendext(uint64_t ext_key, time_point_sec expires);

        ACTION closeext(uint64_t ext_key);

        ACTION lockext(uint64_t ext_key, bool locked);

        ACTION clean();

        struct [[eosio::table("escrows"), eosio::contract("escrow")]] escrow_info {
            uint64_t        key;
            name            sender;
            name            receiver;
            name            approver;
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

    private:
        escrows_table escrows;
        name sending_code;

        std::optional<uint64_t> key_for_external_key(std::optional<uint64_t> ext_key);

        // 6 months in seconds (Computatio: 6 months * average days per month * 24 hours * 60 minutes * 60 seconds)
        constexpr static uint32_t SIX_MONTHS_IN_SECONDS = (uint32_t) (6 * (365.25 / 12) * 24 * 60 * 60);
    };
};
