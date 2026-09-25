const COUNT: usize = 3usize;
const READY: bool = true;

fn scalar(flag: bool, mut number: i32) -> i32 {
    let unit: () = ();
    let unsigned: u32 = 42u32;
    number += 1i32;
    if flag && READY {
        number + COUNT as i32
    } else {
        unsigned as i32
    }
}
