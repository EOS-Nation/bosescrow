#include "escrow.hpp"

namespace bos {

escrow::~escrow() {}

[[eosio::on_notify("eosio.token::transfer")]]
void escrow::transfer(name from,
                            name to,
                            asset quantity,
                            string memo) {

    if (to != _self){
        return;
    }

    require_auth(from);

    auto by_sender = escrows.get_index<"bysender"_n>();

    uint8_t found = 0;

    for (auto esc_itr = by_sender.lower_bound(from.value), end_itr = by_sender.upper_bound(from.value); esc_itr != end_itr; ++esc_itr) {
        if (esc_itr->ext_asset.quantity.amount == 0){

            by_sender.modify(esc_itr, from, [&](escrow_info &e) {
                e.ext_asset = extended_asset{quantity, sending_code};
            });

            found = 1;

            break;
        }
    }

    check(found, "Could not find existing escrow to deposit to, transfer cancelled");
}

ACTION escrow::init(name sender, name receiver, name approver, time_point_sec expires, string memo, std::optional<uint64_t> ext_reference ) {
    check( sender != receiver, "cannot escrow to self" );
    check( receiver != approver, "receiver cannot be approver" );
    require_auth(sender);
    check( is_account( receiver ), "receiver account does not exist");
    check( is_account( approver ), "approver account does not exist");

    // Validate expire time_point_sec
    check(expires > current_time_point(), "expires must be a value in the future.");
    time_point_sec max_expires = current_time_point() + time_point_sec(SIX_MONTHS_IN_SECONDS);
    check(expires <= max_expires, "expires must be within 6 months from now.");

    // Ensure sender is BOS Executive
    check(
        sender == name("bet.bos"),
        "Only BOS Executive can create an escrow."
    );

    // Ensure approve is BOS Executive or eosio
    check(
        approver == name("bet.bos") ||
        approver == name("eosio"),
        "Approver must be BOS Executive or EOSIO."
    );

    // Notify the following accounts
    require_recipient( sender );
    require_recipient( receiver );
    require_recipient( approver );

    extended_asset zero_asset{{0, symbol{"BOS", 4}}, "eosio.token"_n};

    auto by_sender = escrows.get_index<"bysender"_n>();

    for (auto esc_itr = by_sender.lower_bound(sender.value), end_itr = by_sender.upper_bound(sender.value); esc_itr != end_itr; ++esc_itr) {
        check(esc_itr->ext_asset.quantity.amount != 0, "You already have an empty escrow.  Either fill it or delete it");
    }

    if (ext_reference) {
        print("Has external reference: ", ext_reference.value());
        check(!key_for_external_key(*ext_reference),
                        "Already have an escrow with this external reference");
    }

    escrows.emplace(sender, [&](escrow_info &p) {
        p.key = escrows.available_primary_key();
        p.sender = sender;
        p.receiver = receiver;
        p.approver = approver;
        p.ext_asset = zero_asset;
        p.expires = expires;
        p.memo = memo;
        p.locked = false;
        if (!ext_reference) {
            p.external_reference = -1;
        } else {
            p.external_reference = *ext_reference;
        }
    });
}

ACTION escrow::approve(uint64_t key, name approver) {
    require_auth(approver);

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");

    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    check(esc_itr->sender == approver || esc_itr->approver == approver, "You are not allowed to approve this escrow.");

    auto approvals = esc_itr->approvals;
    check(std::find(approvals.begin(), approvals.end(), approver) == approvals.end(), "You have already approved this escrow");

    escrows.modify(esc_itr, approver, [&](escrow_info &e){
        // if approver is bet.bos, no change, allow proposer to claim 100% of the fund
        // if approver is BPs, only keep 90% fund for proposer to claim, and BET.BOS will manually execute transfer ACTION in escrow.bos to send fund to each BPs and each auditors
        if (approver == name("eosio")) {
            e.ext_asset.quantity.amount = e.ext_asset.quantity.amount * 0.90;
        }
        e.approvals.push_back(approver);
    });
}

ACTION escrow::approveext(uint64_t ext_key, name approver) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    approve(*key, approver);
}

ACTION escrow::unapprove(uint64_t key, name disapprover) {
    require_auth(disapprover);

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");

    escrows.modify(esc_itr, name{0}, [&](escrow_info &e){
        auto existing = std::find(e.approvals.begin(), e.approvals.end(), disapprover);
        check(existing != e.approvals.end(), "You have NOT approved this escrow");
        e.approvals.erase(existing);
    });
}

ACTION escrow::unapproveext(uint64_t ext_key, name unapprover) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    unapprove(*key, unapprover);
}

ACTION escrow::claim(uint64_t key) {

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");

    // require_auth(esc_itr->receiver);

    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    check(esc_itr->locked == false, "This escrow has been locked by the approver");

    auto approvals = esc_itr->approvals;

    check(approvals.size() >= 1, "This escrow has not received the required approvals to claim");

    // inline transfer the required funds
    eosio::action(
            eosio::permission_level{_self , "active"_n },
            esc_itr->ext_asset.contract, "transfer"_n,
            make_tuple( _self, esc_itr->receiver, esc_itr->ext_asset.quantity, esc_itr->memo)
    ).send();

    escrows.erase(esc_itr);
}

ACTION escrow::claimext(uint64_t ext_key) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    print("found key to approve :", key.value());
    claim(*key);
}

/*
    * Empties an unfilled escrow request
    */
ACTION escrow::cancel(uint64_t key) {

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");

    require_auth(esc_itr->sender);

    check(0 == esc_itr->ext_asset.quantity.amount, "Amount is not zero, this escrow is locked down");

    escrows.erase(esc_itr);
}

ACTION escrow::cancelext(uint64_t ext_key) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    print("found key to approve :", key.value());
    cancel(*key);
}

/*
    * Allows the sender to withdraw the funds if there are not enough approvals and the escrow has expired
    */
ACTION escrow::refund(uint64_t key) {

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");

    require_auth(esc_itr->sender);

    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    check(esc_itr->locked == false, "This escrow has been locked by the approver");

    time_point_sec time_now = time_point_sec(current_time_point());

    check(time_now >= esc_itr->expires, "Escrow has not expired");
    // check(esc_itr->approvals.size() >= 2, "Escrow has not received the required number of approvals");


    eosio::action(
            eosio::permission_level{_self , "active"_n }, esc_itr->ext_asset.contract, "transfer"_n,
            make_tuple( _self, esc_itr->sender, esc_itr->ext_asset.quantity, esc_itr->memo)
    ).send();


    escrows.erase(esc_itr);
}

ACTION escrow::refundext(uint64_t ext_key) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    print("found key to approve :", key.value());
    refund(*key);
}

/*
    * Allows the sender to extend the expiry
    */
ACTION escrow::extend(uint64_t key, time_point_sec expires) {

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");
    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    time_point_sec time_now = time_point_sec(current_time_point());

    // approver may extend or shorten the time
    // the sender may only extend
    if(has_auth(esc_itr->sender)) {
        check(expires > esc_itr->expires, "You may only extend the expiry");
    } else {
        require_auth(esc_itr->approver);
    }

    escrows.modify(esc_itr, eosio::same_payer, [&](escrow_info &e){
        e.expires = expires;
    });
}

ACTION escrow::extendext(uint64_t ext_key, time_point_sec expires) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    print("found key to approve :", key.value());
    extend(*key,expires);
}


/*
    * Allows the approver to close and refund an unexpired escrow
    */
ACTION escrow::close(uint64_t key) {
    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");

    require_auth(esc_itr->approver);
    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    eosio::action(
            eosio::permission_level{_self , "active"_n }, esc_itr->ext_asset.contract, "transfer"_n,
            make_tuple( _self, esc_itr->sender, esc_itr->ext_asset.quantity, esc_itr->memo)
    ).send();

    escrows.erase(esc_itr);
}

ACTION escrow::closeext(uint64_t ext_key) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    print("found key to approve :", key.value());
    close(*key);
}

/*
    * Allows the approver to lock an escrow preventing any actions by sender or receiver
    */
ACTION escrow::lock(uint64_t key, bool locked) {

    auto esc_itr = escrows.find(key);
    check(esc_itr != escrows.end(), "Could not find escrow with that index");
    require_auth(esc_itr->approver);
    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    escrows.modify(esc_itr, eosio::same_payer, [&](escrow_info &e){
        e.locked = locked;
    });

}

ACTION escrow::lockext(uint64_t ext_key, bool locked) {
    auto key = key_for_external_key(ext_key);
    check(key.has_value(), "No escrow exists for this external key.");
    print("found key to approve :", key.value());
    lock(*key,locked);
}

ACTION escrow::clean() {
    require_auth(_self);

    auto itr = escrows.begin();
    while (itr != escrows.end()){
        itr = escrows.erase(itr);
    }
}

// private helper

std::optional<uint64_t> escrow::key_for_external_key(std::optional<uint64_t> ext_key) {

    if (!ext_key.has_value()) {
        return std::nullopt;
    }

    auto by_external_ref = escrows.get_index<"byextref"_n>();

    for (auto esc_itr = by_external_ref.lower_bound(ext_key.value()), end_itr = by_external_ref.upper_bound(ext_key.value()); esc_itr != end_itr; ++esc_itr) {
        print("found a match key");
        return esc_itr->key;
    }
    print("no match key");
    return std::nullopt;
}
} /// namespace bos

// #define EOSIO_ABI_EX(TYPE, MEMBERS) \
// extern "C" { \
//    void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
//       if( action == "onerror"_n.value) { \
//          /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
//          check(code == "eosio"_n.value, "onerror action's are only valid from the \"eosio\" system account"); \
//       } \
//       auto self = receiver; \
//       if( (code == self  && action != "transfer"_n.value) || (action == "transfer"_n.value) ) { \
//          switch( action ) { \
//             EOSIO_DISPATCH_HELPER( TYPE, MEMBERS ) \
//          } \
//          /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
//       } \
//    } \
// }

// EOSIO_DISPATCH(bos::escrow,
//               (transfer)
//               (init)
//               (approve)(approveext)
//               (unapprove)(unapproveext)
//               (claim)(claimext)
//               (refund)(refundext)
//               (cancel)(cancelext)
//               (extend)(extendext)
//               (close)(closeext)
//               (lock)(lockext)
//               (clean)
// )
