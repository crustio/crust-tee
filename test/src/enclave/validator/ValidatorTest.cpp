#include "ValidatorTest.h"
#include "Identity.h"
#include "Srd.h"
#include <algorithm>

extern crust_status_t validate_real_file(uint8_t *p_sealed_data, size_t sealed_data_size);
std::string oneg_cid = "";

void validate_srd_bench()
{
    if (ENC_UPGRADE_STATUS_SUCCESS == Workload::get_instance()->get_upgrade_status())
    {
        return;
    }

    crust_status_t crust_status = CRUST_SUCCESS;

    Workload *wl = Workload::get_instance();

    sgx_thread_mutex_lock(&wl->srd_mutex);
    size_t srd_validate_num = std::max((size_t)(wl->srd_hashs.size() * SRD_VALIDATE_RATE), (size_t)SRD_VALIDATE_MIN_NUM);
    srd_validate_num = std::min(srd_validate_num, wl->srd_hashs.size());
    
    // Randomly choose validate srd files
    std::map<int, uint8_t *> validate_srd_m;
    uint32_t rand_val;
    uint32_t rand_idx = 0;
    if (srd_validate_num >= wl->srd_hashs.size())
    {
        for (size_t i = 0; i < wl->srd_hashs.size(); i++)
        {
            validate_srd_m[i] = wl->srd_hashs[i];
        }
    }
    else
    {
        for (size_t i = 0; i < srd_validate_num; i++)
        {
            sgx_read_rand((uint8_t *)&rand_val, 4);
            rand_idx = rand_val % wl->srd_hashs.size();
            validate_srd_m[rand_idx] = wl->srd_hashs[rand_idx];
        }
    }
    uint32_t g_hash_index = 0;
    uint8_t *p_g_hash = wl->srd_hashs[g_hash_index];
    sgx_thread_mutex_unlock(&wl->srd_mutex);

    // ----- Validate SRD ----- //
    std::vector<uint8_t *> del_hashs;
    for (auto idx2srd : validate_srd_m)
    {
        g_hash_index = idx2srd.first;
        g_hash_index = 0;
        uint8_t *m_hashs_org = NULL;
        uint8_t *m_hashs = NULL;
        size_t m_hashs_size = 0;
        sgx_sha256_hash_t m_hashs_sha256;
        size_t srd_block_index = 0;
        std::string leaf_path;
        uint8_t *leaf_data = NULL;
        size_t leaf_data_len = 0;
        sgx_sha256_hash_t leaf_hash;
        std::string g_hash;

        // Get g_hash corresponding path
        g_hash = hexstring_safe(p_g_hash, HASH_LENGTH);

        // Get M hashs
        crust_status = srd_get_file(get_m_hashs_file_path(g_hash.c_str()).c_str(), &m_hashs_org, &m_hashs_size);
        if (m_hashs_org == NULL)
        {
            if (!wl->add_srd_to_deleted_buffer(g_hash_index))
            {
                goto nextloop;
            }
            log_err("Get m hashs file(%s) failed.\n", g_hash.c_str());
            del_hashs.push_back(p_g_hash);
            goto nextloop;
        }

        m_hashs = (uint8_t *)enc_malloc(m_hashs_size);
        if (m_hashs == NULL)
        {
            log_err("Malloc memory failed!\n");
            goto nextloop;
        }
        memset(m_hashs, 0, m_hashs_size);
        memcpy(m_hashs, m_hashs_org, m_hashs_size);

        // Compare M hashs
        sgx_sha256_msg(m_hashs, m_hashs_size, &m_hashs_sha256);
        if (memcmp(p_g_hash, m_hashs_sha256, HASH_LENGTH) != 0)
        {
            log_err("Wrong m hashs file(%s).\n", g_hash.c_str());
            del_hashs.push_back(p_g_hash);
            wl->add_srd_to_deleted_buffer(g_hash_index);
            goto nextloop;
        }

        // Get leaf data
        sgx_read_rand((uint8_t*)&rand_val, 4);
        srd_block_index = rand_val % SRD_RAND_DATA_NUM;
        leaf_path = get_leaf_path(g_hash.c_str(), srd_block_index, m_hashs + srd_block_index * HASH_LENGTH);
        crust_status = srd_get_file(leaf_path.c_str(), &leaf_data, &leaf_data_len);

        if (leaf_data == NULL)
        {
            if (!wl->add_srd_to_deleted_buffer(g_hash_index))
            {
                goto nextloop;
            }
            log_err("Get leaf file(%s) failed.\n", g_hash.c_str());
            del_hashs.push_back(p_g_hash);
            goto nextloop;
        }

        // Compare leaf data
        sgx_sha256_msg(leaf_data, leaf_data_len, &leaf_hash);
        if (memcmp(m_hashs + srd_block_index * HASH_LENGTH, leaf_hash, HASH_LENGTH) != 0)
        {
            log_err("Wrong leaf data hash '%s'(file path:%s).\n", g_hash.c_str(), g_hash.c_str());
            del_hashs.push_back(p_g_hash);
            wl->add_srd_to_deleted_buffer(g_hash_index);
            goto nextloop;
        }


    nextloop:
        if (m_hashs != NULL)
        {
            free(m_hashs);
        }

        if (m_hashs_org != NULL)
        {
            free(m_hashs_org);
        }

        if (leaf_data != NULL)
        {
            free(leaf_data);
        }
    }

    // Delete failed srd metadata
    for (auto hash : del_hashs)
    {
        std::string del_path = hexstring_safe(hash, HASH_LENGTH);
        ocall_delete_folder_or_file(&crust_status, del_path.c_str(), STORE_TYPE_SRD);
    }
}

void validate_srd_test()
{
    if (ENC_UPGRADE_STATUS_SUCCESS == Workload::get_instance()->get_upgrade_status())
    {
        return;
    }

    crust_status_t crust_status = CRUST_SUCCESS;

    Workload *wl = Workload::get_instance();

    sgx_thread_mutex_lock(&wl->srd_mutex);
    size_t srd_validate_num = std::max((size_t)(wl->srd_hashs.size() * SRD_VALIDATE_RATE), (size_t)SRD_VALIDATE_MIN_NUM);
    srd_validate_num = std::min(srd_validate_num, wl->srd_hashs.size());
    
    // Randomly choose validate srd files
    std::map<int, uint8_t *> validate_srd_m;
    uint32_t rand_val;
    uint32_t rand_idx = 0;
    if (srd_validate_num >= wl->srd_hashs.size())
    {
        for (size_t i = 0; i < wl->srd_hashs.size(); i++)
        {
            validate_srd_m[i] = wl->srd_hashs[i];
        }
    }
    else
    {
        for (size_t i = 0; i < srd_validate_num; i++)
        {
            sgx_read_rand((uint8_t *)&rand_val, 4);
            rand_idx = rand_val % wl->srd_hashs.size();
            validate_srd_m[rand_idx] = wl->srd_hashs[rand_idx];
        }
    }
    sgx_thread_mutex_unlock(&wl->srd_mutex);

    // ----- Validate SRD ----- //
    std::vector<uint8_t *> del_hashs;
    for (auto idx2srd : validate_srd_m)
    {
        uint8_t *m_hashs_org = NULL;
        uint8_t *m_hashs = NULL;
        uint8_t *leaf_data = NULL;
        std::string g_hash;

        uint8_t *p_g_hash = idx2srd.second;

        // Get g_hash corresponding path
        g_hash = hexstring_safe(p_g_hash, HASH_LENGTH);

        // Get M hashs

        // Compare M hashs

        // Get leaf data

        // Compare leaf data


        if (m_hashs != NULL)
        {
            free(m_hashs);
        }

        if (m_hashs_org != NULL)
        {
            free(m_hashs_org);
        }

        if (leaf_data != NULL)
        {
            free(leaf_data);
        }
    }

    // Delete failed srd metadata
    for (auto hash : del_hashs)
    {
        std::string del_path = hexstring_safe(hash, HASH_LENGTH);
        ocall_delete_folder_or_file(&crust_status, del_path.c_str(), STORE_TYPE_SRD);
    }
}

void validate_meaningful_file_bench()
{
    if (ENC_UPGRADE_STATUS_SUCCESS == Workload::get_instance()->get_upgrade_status())
    {
        return;
    }

    uint8_t *p_data = NULL;
    size_t data_len = 0;
    crust_status_t crust_status = CRUST_SUCCESS;
    Workload *wl = Workload::get_instance();

    // Lock wl->sealed_files
    sgx_thread_mutex_lock(&wl->file_mutex);
    // Get to be checked files indexes
    size_t check_file_num = std::max((size_t)(wl->sealed_files.size() * MEANINGFUL_VALIDATE_RATE), (size_t)MEANINGFUL_VALIDATE_MIN_NUM);
    check_file_num = std::min(check_file_num, wl->sealed_files.size());
    std::map<uint32_t, json::JSON> validate_sealed_files_m;
    uint32_t rand_val;
    size_t rand_index = 0;
    if (check_file_num >= wl->sealed_files.size())
    {
        for (size_t i = 0; i < wl->sealed_files.size(); i++)
        {
            validate_sealed_files_m[i] = wl->sealed_files[i];
        }
    }
    else
    {
        for (size_t i = 0; i < check_file_num; i++)
        {
            sgx_read_rand((uint8_t *)&rand_val, 4);
            rand_index = rand_val % wl->sealed_files.size();
            validate_sealed_files_m[rand_index] = wl->sealed_files[rand_index];
        }
    }
    if (oneg_cid.compare("") == 0)
    {
        long target_size_min = 1024 * 1024 * 1024;
        long target_size_max = (100 + 1024) * 1024 * 1024;
        for (auto el : wl->sealed_files)
        {
            if (el[FILE_SIZE].ToInt() >= target_size_min && el[FILE_SIZE].ToInt() <= target_size_max)
            {
                oneg_cid = el[FILE_CID].ToString();
                break;
            }
        }
    }
    size_t file_idx = 0;
    wl->is_file_dup(oneg_cid, file_idx);
    json::JSON file = wl->sealed_files[file_idx];
    sgx_thread_mutex_unlock(&wl->file_mutex);

    // ----- Validate file ----- //
    // Used to indicate which meaningful file status has been changed
    std::unordered_set<uint32_t> deleted_idx_us;
    for (auto idx2file : validate_sealed_files_m)
    {
        // If new file hasn't been verified, skip this validation
        auto status = file[FILE_STATUS];
        if (status.get_char(CURRENT_STATUS) == FILE_STATUS_PENDING
                || status.get_char(CURRENT_STATUS) == FILE_STATUS_DELETED)
        {
            continue;
        }

        std::string root_cid = file[FILE_CID].ToString();
        std::string root_hash = file[FILE_HASH].ToString();
        size_t file_block_num = file[FILE_BLOCK_NUM].ToInt();
        // Get tree string
        crust_status = persist_get_unsafe(root_cid, &p_data, &data_len);
        if (CRUST_SUCCESS != crust_status)
        {
            if (wl->is_in_deleted_file_buffer(root_cid))
            {
                continue;
            }
            log_err("Validate meaningful data failed! Get tree:%s failed!\n", root_cid.c_str());
            if (status.get_char(CURRENT_STATUS) == FILE_STATUS_VALID)
            {
                deleted_idx_us.insert(file_idx);
            }
            if (p_data != NULL)
            {
                free(p_data);
                p_data = NULL;
            }
            continue;
        }
        // Validate merkle tree
        std::string tree_str(reinterpret_cast<const char *>(p_data), data_len);
        if (p_data != NULL)
        {
            free(p_data);
            p_data = NULL;
        }
        json::JSON tree_json = json::JSON::Load(tree_str);
        bool valid_tree = true;
        if (root_hash.compare(tree_json[MT_HASH].ToString()) != 0)
        {
            log_err("File:%s merkle tree is not valid!Root hash doesn't equal!\n", root_cid.c_str());
            valid_tree = false;
        }
        if (CRUST_SUCCESS != (crust_status = validate_merkletree_json(tree_json)))
        {
            log_err("File:%s merkle tree is not valid!Invalid merkle tree,error code:%lx\n", root_cid.c_str(), crust_status);
            valid_tree = false;
        }
        if (!valid_tree)
        {
            if (status.get_char(CURRENT_STATUS) == FILE_STATUS_VALID)
            {
                deleted_idx_us.insert(file_idx);
            }
            continue;
        }

        // ----- Validate MerkleTree ----- //
        // Get to be checked block index
        std::set<size_t> block_idx_s;
        size_t tmp_idx = 0;
        for (size_t i = 0; i < MEANINGFUL_VALIDATE_MIN_BLOCK_NUM && i < file_block_num; i++)
        {
            sgx_read_rand((uint8_t *)&rand_val, 4);
            tmp_idx = rand_val % file_block_num;
            if (block_idx_s.find(tmp_idx) == block_idx_s.end())
            {
                block_idx_s.insert(tmp_idx);
            }
        }
        // Do check
        // Note: should store serialized tree structure as "cid":x,"hash":"xxxxx"
        // be careful to keep "cid", "hash" sequence
        size_t pos = 0;
        std::string dhash_tag(MT_DATA_HASH "\":\"");
        size_t cur_block_idx = 0;
        for (auto check_block_idx : block_idx_s)
        {
            // Get leaf node position
            do
            {
                pos = tree_str.find(dhash_tag, pos);
                if (pos == tree_str.npos)
                {
                    break;
                }
                pos += dhash_tag.size();
            } while (cur_block_idx++ < check_block_idx);
            if (pos == tree_str.npos)
            {
                log_err("Find file(%s) leaf node cid failed!node index:%ld\n", root_cid.c_str(), check_block_idx);
                if (status.get_char(CURRENT_STATUS) == FILE_STATUS_VALID)
                {
                    deleted_idx_us.insert(file_idx);
                }
                break;
            }
            // Get current node hash
            std::string leaf_hash = tree_str.substr(pos, HASH_LENGTH * 2);
            // Compute current node hash by data
            uint8_t *p_sealed_data = NULL;
            size_t sealed_data_size = 0;
            std::string leaf_path = root_cid + "/" + leaf_hash;
            crust_status = storage_get_file(leaf_path.c_str(), &p_sealed_data, &sealed_data_size);
            if (CRUST_SUCCESS != crust_status)
            {
                if (p_sealed_data != NULL)
                {
                    free(p_sealed_data);
                }
                if (wl->is_in_deleted_file_buffer(root_cid))
                {
                    continue;
                }
                if (status.get_char(CURRENT_STATUS) == FILE_STATUS_VALID)
                {
                    deleted_idx_us.insert(file_idx);
                }
                log_err("Get file(%s) block:%ld failed!\n", root_cid.c_str(), check_block_idx);
                break;
            }
            // Validate sealed hash
            sgx_sha256_hash_t got_hash;
            sgx_sha256_msg(p_sealed_data, sealed_data_size, &got_hash);
            uint8_t *leaf_hash_u = hex_string_to_bytes(leaf_hash.c_str(), leaf_hash.size());
            if (leaf_hash_u == NULL)
            {
                log_warn("Validate: Hexstring to bytes failed!Skip block:%ld check.\n", check_block_idx);
                free(p_sealed_data);
                continue;
            }
            int memret = memcmp(leaf_hash_u, got_hash, HASH_LENGTH);
            free(leaf_hash_u);
            if (memret != 0)
            {
                if (status.get_char(CURRENT_STATUS) == FILE_STATUS_VALID)
                {
                    log_err("File(%s) Index:%ld block hash is not expected!\n", root_cid.c_str(), check_block_idx);
                    log_err("Get hash : %s\n", hexstring(got_hash, HASH_LENGTH));
                    log_err("Org hash : %s\n", leaf_hash.c_str());
                    deleted_idx_us.insert(file_idx);
                }
                free(p_sealed_data);
                break;
            }
            // Validate real file piece
            crust_status = validate_real_file(p_sealed_data, sealed_data_size);
            free(p_sealed_data);
            if (CRUST_SUCCESS != crust_status)
            {
                if (CRUST_SERVICE_UNAVAILABLE == crust_status)
                {
                    wl->set_report_file_flag(false);
                    goto cleanup;
                }
                else
                {
                    log_err("Get file(%s) block failed! Error code:%lx\n", root_cid.c_str(), crust_status);
                    deleted_idx_us.insert(file_idx);
                }
                break;
            }
        }
    }

cleanup:
    // Change file status
    if (deleted_idx_us.size() > 0)
    {
        sgx_thread_mutex_lock(&wl->file_mutex);
        for (auto del_index : deleted_idx_us)
        {
            size_t index = 0;
            std::string cid = validate_sealed_files_m[del_index][FILE_CID].ToString();
            if(wl->is_file_dup(cid, index))
            {
                long cur_block_num = wl->sealed_files[index][CHAIN_BLOCK_NUMBER].ToInt();
                long val_block_num = validate_sealed_files_m[del_index][CHAIN_BLOCK_NUMBER].ToInt();
                // We can get original file and new sealed file(this situation maybe exist)
                // If these two files are not the same one, cannot delete the file
                if (cur_block_num == val_block_num)
                {
                    log_info("File status changed, hash: %s status: valid -> lost, will be deleted\n", cid.c_str());
                    // Change file status
                    wl->sealed_files[index][FILE_STATUS].set_char(CURRENT_STATUS, FILE_STATUS_DELETED);
                    // Delete real file
                    ocall_ipfs_del_all(&crust_status, cid.c_str());
                    // Reduce valid file
                    wl->set_wl_spec(FILE_STATUS_VALID, -validate_sealed_files_m[index][FILE_SIZE].ToInt());
                }
            }
            else
            {
                log_err("Deal with bad file(%s) failed!\n", cid.c_str());
            }
        }
        sgx_thread_mutex_unlock(&wl->file_mutex);
    }
}
