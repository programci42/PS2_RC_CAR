#ifndef RELAY_POLICY_H
#define RELAY_POLICY_H

#define RELAY_COUNT 8
#define RELAY_GROUP_COUNT 4
#define STARTUP_PULSE_MS 100

static int valid_group(int group) { return group >= 0 && group < RELAY_GROUP_COUNT; }

static void normalize_groups(int *left, int *right)
{
    if (!valid_group(*left)) *left = 0;
    if (!valid_group(*right) || *right == *left) *right = (*left + 1) % RELAY_GROUP_COUNT;
}

static int next_group(int current, int other, int direction)
{
    int candidate = current;
    do { candidate = (candidate + (direction < 0 ? 3 : 1)) % RELAY_GROUP_COUNT; } while (candidate == other);
    return candidate;
}

static void motor_relay_mask(int desired[RELAY_COUNT], int left, int right,
                             int lf, int lr, int rf, int rr)
{
    int i;
    for (i = 0; i < RELAY_COUNT; i++) desired[i] = 0;
    if (!valid_group(left) || !valid_group(right) || left == right) return;
    desired[left * 2] = lf && !lr;
    desired[left * 2 + 1] = lr && !lf;
    desired[right * 2] = rf && !rr;
    desired[right * 2 + 1] = rr && !rf;
}

#endif
