fn classify(value: i32) -> i32 {
    ;
    let adjusted = { value + 1i32 };
    if adjusted < 0i32 {
        -1i32
    } else if adjusted == 0i32 {
        0i32
    } else {
        adjusted
    }
}
