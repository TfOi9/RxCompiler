fn calculate(mut left: i32, right: i32) -> i32 {
    left += right * 2i32;
    left -= 1i32;
    left *= 1i32;
    left /= 1i32;
    left %= 3i32;
    left &= right;
    left |= right;
    left ^= 2i32;
    left <<= 1i32;
    left >>= 1i32;
    let negative = -left;
    let quotient = (left / right) % 3i32;
    let bits = (left & right) ^ quotient;
    let shifted = ((left << 1i32) | bits) >> 1i32;
    let matches = (left >= right || left != right) && left <= right;
    if matches { negative } else { shifted }
}
