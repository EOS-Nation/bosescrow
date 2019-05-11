#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <optional>
#include "escrow_shared.hpp"

using namespace eosio;
using namespace std;

namespace bos {
    class escrow : public contract {

    private:
        escrows_table escrows;
        name sending_code;

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

        ACTION transfer(name from, name to, asset quantity, string memo);

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

    private:
        std::optional<uint64_t> key_for_external_key(std::optional<uint64_t> ext_key);
    };
};
