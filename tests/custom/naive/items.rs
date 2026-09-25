#[derive(Copy, Clone, PartialEq, Eq,)]
struct Pair {
    left: i32,
    right: i32,
}

const LOWER: i32 = -((1i32));
const NEGATED_LOWER: i32 = -LOWER;

fn make_pair(left: i32, right: i32) -> Pair {
    Pair { left: left, right: right }
}
