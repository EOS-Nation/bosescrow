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

class [[eosio::contract("escrow")]] escrow : public eosio::contract {
    public:

        escrow(name s, name code, datastream<const char *> ds)
                : contract(s, code, ds),
                  escrows(_self, _self.value) {
            sending_code = name{code};
        }

        ~escrow();

        [[eosio::on_notify("eosio.token::transfer")]]
        void transfer(name from, name to, asset quantity, string memo);

        [[eosio::action]]
        void init(
            const name           sender,
            const name           receiver,
            const name           approver,
            const time_point_sec expires,
            const string         memo,
            const std::optional<uint64_t> ext_reference
        );

        [[eosio::action]]
        void approve(const uint64_t key, const name approver);

        [[eosio::action]]
        void unapprove(const uint64_t key, const name unapprover);

        [[eosio::action]]
        void claim(const uint64_t key);

        [[eosio::action]]
        void refund(const uint64_t key);

        [[eosio::action]]
        void cancel(const uint64_t key);

        [[eosio::action]]
        void extend(const uint64_t key, const time_point_sec expires);

        [[eosio::action]]
        void close(const uint64_t key);

        [[eosio::action]]
        void lock(const uint64_t key, const bool locked);

        // Actions using the external reference key
        [[eosio::action]]
        void approveext(const uint64_t ext_key, const name approver);

        [[eosio::action]]
        void unapproveext(const uint64_t ext_key, const name unapprover);

        [[eosio::action]]
        void claimext(const uint64_t ext_key);

        [[eosio::action]]
        void refundext(const uint64_t ext_key);

        [[eosio::action]]
        void cancelext(const uint64_t ext_key);

        [[eosio::action]]
        void extendext(const uint64_t ext_key, const time_point_sec expires);

        [[eosio::action]]
        void closeext(const uint64_t ext_key);

        [[eosio::action]]
        void lockext(const uint64_t ext_key, const bool locked);

        [[eosio::action]]
        void clean();

    private:
        // 6 months in seconds (Computatio: 6 months * average days per month * 24 hours * 60 minutes * 60 seconds)
        constexpr static uint32_t SIX_MONTHS_IN_SECONDS = (uint32_t) (6 * (365.25 / 12) * 24 * 60 * 60);

        struct [[eosio::table]] escrow_info {
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

        escrows_table escrows;
        name sending_code;

        std::optional<uint64_t> key_for_external_key(std::optional<uint64_t> ext_key);
};
