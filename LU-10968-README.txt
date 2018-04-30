# export LUSTRE=/root/lustre-release/lustre
# $LUSTRE/tests/llmount.sh

export HSM_ROOT1=/tmp/arc1
# rm -rf $HSM_ROOT1

lctl set_param mdt.*.hsm.upcall_mask='ARCHIVE'
lctl set_param mdt.*.hsm.upcall_path="$LUSTRE/utils/lhsm_mdt_upcall"

lctl set_param mdt.*.hsm_control=enabled
mkdir -p /mnt/lustre-hsm1
mkdir -p $HSM_ROOT1

mount $HOSTNAME@tcp:/lustre /mnt/lustre-hsm1 -t lustre -o user_xattr,flock

$LUSTRE/utils/lhsmtool_posix --bandwidth=1 --no-shadow -vvvv --archive=1 \
  --hsm_root=$HSM_ROOT1 --daemon /mnt/lustre-hsm1 2> /tmp/hsm1.log

echo XXX | tee /mnt/lustre/f{0..31}
lfs hsm_archive /mnt/lustre/f{0..31}

cat /tmp/hsm_upcall_actions
cat /tmp/hsm_upcall_errors

# LHSM_ARCHIVE_BASE=/tmp/arc is set by default in lhsm_worker_posix
# assumes archive 1 is at /tmp/arc1, archive 2 is at /tmp/arc2, ...

# lfs hsm_upcall $LUSTRE/utils/lhsm_worker_posix ARCHIVE lustre 1 0 '' "[0x200000401:0x1:0x0]"

while read line; do
  lfs hsm_upcall $LUSTRE/utils/lhsm_worker_posix $line
done < /tmp/hsm_upcall_actions

: > /tmp/hsm_archive_actions

lfs hsm_state /mnt/lustre/f{0..31}
