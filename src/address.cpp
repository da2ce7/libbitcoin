#include <bitcoin/address.hpp>

#include <bitcoin/constants.hpp>
#include <bitcoin/format.hpp>
#include <bitcoin/utility/base58.hpp>
#include <bitcoin/utility/ripemd.hpp>
#include <bitcoin/utility/sha256.hpp>

namespace libbitcoin {

payment_address::payment_address()
  : type_(payment_type::non_standard)
{
}
payment_address::payment_address(
    payment_type address_type, const short_hash& hash)
{
    set(address_type, hash);
}
payment_address::payment_address(const std::string& encoded_address)
{
    set_encoded(encoded_address);
}

bool payment_address::set(payment_type address_type, const short_hash& hash)
{
    type_ = address_type;
    hash_ = hash;
    return true;
}

bool payment_address::set_raw(byte version_byte, const short_hash& hash)
{
    switch (version_byte)
    {
        case pubkey_version:
            type_ = payment_type::pubkey_hash;
            hash_ = hash;
            return true;

        case script_version:
            type_ = payment_type::script_hash;
            hash_ = hash;
            return true;

        default:
            return false;
    }
}

const short_hash& payment_address::hash() const
{
    return hash_;
}

payment_type payment_address::type() const
{
    return type_;
}

bool payment_address::set_encoded(const std::string& encoded_address)
{
    const data_chunk decoded_address = decode_base58(encoded_address);
    // version + 20 bytes short hash + 4 bytes checksum
    if (decoded_address.size() != 25)
        return false;
    const byte version = decoded_address[0];
    if (!set_version(version))
        return false;
    const data_chunk checksum_bytes(
        decoded_address.end() - 4, decoded_address.end());
    // version + short hash
    const data_chunk main_body(
        decoded_address.begin(), decoded_address.end() - 4);
    // verify checksum bytes
    if (generate_sha256_checksum(main_body) != 
            cast_chunk<uint32_t>(checksum_bytes))
        return false;
    std::copy(main_body.begin() + 1, main_body.end(), hash_.begin());
    return true;
}

std::string payment_address::encoded() const
{
    data_chunk unencoded_address;
    BITCOIN_ASSERT(
        type_ == payment_type::pubkey_hash ||
        type_ == payment_type::script_hash);
    // Type, Hash, Checksum doth make thy address
    unencoded_address.push_back(version());
    extend_data(unencoded_address, hash_);
    uint32_t checksum = generate_sha256_checksum(unencoded_address);
    extend_data(unencoded_address, uncast_type(checksum));
    BITCOIN_ASSERT(unencoded_address.size() == 25);
    return encode_base58(unencoded_address);
}

byte payment_address::version() const
{
    switch (type_)
    {
        case payment_type::pubkey_hash:
            return pubkey_version;

        case payment_type::script_hash:
            return script_version;

        case payment_type::pubkey:
        case payment_type::multisig:
        case payment_type::non_standard:
        default:
            return std::numeric_limits<byte>::max();
    }
}

bool payment_address::set_version(byte version_byte)
{
    if (version_byte == pubkey_version)
        type_ = payment_type::pubkey_hash;
    else if (version_byte == script_version)
        type_ = payment_type::script_hash;
    else
        return false;
    return true;
}

bool set_public_key_hash(payment_address& address,
    const short_hash& pubkey_hash)
{
    return address.set(payment_type::pubkey_hash, pubkey_hash);
}

bool set_script_hash(payment_address& address,
    const short_hash& script_hash)
{
    return address.set(payment_type::script_hash, script_hash);
}

bool set_public_key(payment_address& address, const data_chunk& public_key)
{
    return address.set(payment_type::pubkey_hash,
        generate_ripemd_hash(public_key));
}

bool set_script(payment_address& address, const script& eval_script)
{
    return address.set(payment_type::script_hash,
        generate_ripemd_hash(save_script(eval_script)));
}

bool extract(payment_address& address, const script& output_script)
{
    data_chunk raw_hash;
    if (output_script.type() == payment_type::pubkey_hash)
    {
        BITCOIN_ASSERT(output_script.operations().size() == 5);
        raw_hash = output_script.operations()[2].data;
    }
    else if (output_script.type() == payment_type::script_hash)
    {
        BITCOIN_ASSERT(output_script.operations().size() == 3);
        raw_hash = output_script.operations()[1].data;
    }
    else
        return false;
    short_hash hash_data;
    BITCOIN_ASSERT(raw_hash.size() == hash_data.size());
    std::copy(raw_hash.begin(), raw_hash.end(), hash_data.begin());
    address.set(output_script.type(), hash_data);
    return true;
}

} // namespace libbitcoin

