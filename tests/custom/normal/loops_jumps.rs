fn accumulate(limit: i32) -> i32 {
    let mut index = 0i32;
    let mut total = 0i32;

    while index < limit {
        index += 1i32;
        if index == 2i32 {
            continue;
        }
        if index > 5i32 {
            break;
        }
        total += index;
    }

    let result = loop {
        if total > 0i32 {
            break total;
        } else {
            break 0i32;
        }
    };
    return result;
}

fn return_without_value(stop: bool) {
    if stop {
        return;
    }
}
