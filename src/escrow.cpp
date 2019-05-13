#include "escrow.hpp"

escrow::~escrow() {}

[[eosio::on_notify("eosio.token::transfer")]]
void escrow::transfer( const name     from,
                       const name     to,
                       const asset    quantity,
                       const string   memo )
{

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

ACTION escrow::init( const name           sender,
                     const name           receiver,
                     const name           approver,
                     const name           escrow_name,
                     const time_point_sec expires_at,
                     const string         memo)
{
    check( sender != receiver, "cannot escrow to self" );
    check( receiver != approver, "receiver cannot be approver" );
    require_auth(sender);
    check( is_account( receiver ), "receiver account does not exist");
    check( is_account( approver ), "approver account does not exist");
    check( escrow_name.length() > 2, "escrow name should be at least 3 characters long.");

    // Validate expire time_point_sec
    check(expires_at > current_time_point(), "expires_at must be a value in the future.");
    time_point_sec max_expires_at = current_time_point() + time_point_sec(SIX_MONTHS_IN_SECONDS);
    check(expires_at <= max_expires_at, "expires_at must be within 6 months from now.");

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

    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr == escrows.end(), "escrow with same name already exists.");

    escrows.emplace(sender, [&](escrow_info &p) {
        p.escrow_name = escrow_name;
        p.sender = sender;
        p.receiver = receiver;
        p.approver = approver;
        p.ext_asset = zero_asset;
        p.expires_at = expires_at;
        p.created_at = current_time_point();
        p.memo = memo;
        p.locked = false;
    });
}

ACTION escrow::approve( const name escrow_name, const name approver )
{
    require_auth(approver);

    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");

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

ACTION escrow::unapprove( const name escrow_name, const name disapprover )
{
    require_auth(disapprover);

    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");

    escrows.modify(esc_itr, name{0}, [&](escrow_info &e){
        auto existing = std::find(e.approvals.begin(), e.approvals.end(), disapprover);
        check(existing != e.approvals.end(), "You have NOT approved this escrow");
        e.approvals.erase(existing);
    });
}

ACTION escrow::claim( const name escrow_name )
{
    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");

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

/**
 * Empties an unfilled escrow request
 */
ACTION escrow::cancel(const name escrow_name)
{
    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");

    require_auth(esc_itr->sender);

    check(0 == esc_itr->ext_asset.quantity.amount, "Amount is not zero, this escrow is locked down");

    escrows.erase(esc_itr);
}

/**
 * Allows the sender to withdraw the funds if there are not enough approvals and the escrow has expired
 */
ACTION escrow::refund(const name escrow_name)
{
    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");

    require_auth(esc_itr->sender);

    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    check(esc_itr->locked == false, "This escrow has been locked by the approver");

    time_point_sec time_now = time_point_sec(current_time_point());

    check(time_now >= esc_itr->expires_at, "Escrow has not expired");

    eosio::action(
            eosio::permission_level{_self , "active"_n }, esc_itr->ext_asset.contract, "transfer"_n,
            make_tuple( _self, esc_itr->sender, esc_itr->ext_asset.quantity, esc_itr->memo)
    ).send();


    escrows.erase(esc_itr);
}

/**
 * Allows the sender to extend the expiry
 */
ACTION escrow::extend(const name escrow_name, const time_point_sec expires_at)
{
    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");
    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    time_point_sec time_now = time_point_sec(current_time_point());

    // approver may extend or shorten the time
    // the sender may only extend
    if(has_auth(esc_itr->sender)) {
        check(expires_at > esc_itr->expires_at, "You may only extend the expiry");
    } else {
        require_auth(esc_itr->approver);
    }

    escrows.modify(esc_itr, eosio::same_payer, [&](escrow_info &e){
        e.expires_at = expires_at;
    });
}

/**
 * Allows the approver to close and refund an unexpired escrow
 */
ACTION escrow::close(const name escrow_name)
{
    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");

    require_auth(esc_itr->approver);
    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    eosio::action(
            eosio::permission_level{_self , "active"_n }, esc_itr->ext_asset.contract, "transfer"_n,
            make_tuple( _self, esc_itr->sender, esc_itr->ext_asset.quantity, esc_itr->memo)
    ).send();

    escrows.erase(esc_itr);
}

/**
 * Allows the approver to lock an escrow preventing any actions by sender or receiver
 */
ACTION escrow::lock(const name escrow_name, const bool locked)
{
    auto esc_itr = escrows.find(escrow_name.value);
    check(esc_itr != escrows.end(), "Could not find escrow with that name");
    require_auth(esc_itr->approver);
    check(esc_itr->ext_asset.quantity.amount > 0, "This has not been initialized with a transfer");

    escrows.modify(esc_itr, eosio::same_payer, [&](escrow_info &e){
        e.locked = locked;
    });
}

ACTION escrow::clean()
{
    require_auth(_self);

    auto itr = escrows.begin();
    while (itr != escrows.end()){
        itr = escrows.erase(itr);
    }
}
