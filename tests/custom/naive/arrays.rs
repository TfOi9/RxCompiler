const LENGTH: usize = 3usize;

fn select(values: [i32; LENGTH], index: usize) -> i32 {
    let repeated: [i32; 3] = [7i32; 3usize];
    let listed = [1i32, 2i32, 3i32,];
    values[index] + repeated[1usize] + listed[2usize]
}
