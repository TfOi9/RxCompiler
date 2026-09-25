use rx::core::{Box, Vec,};

struct Nested<'a> {
    values: Vec<Vec<&'a [i32; 2]>>,
}

fn wrap<'a>(values: Vec<Vec<&'a [i32; 2]>>) -> Nested<'a> {
    Nested { values: values }
}

fn empty_nested() -> Vec<Vec<i32>> {
    Vec::<Vec<i32>>::new()
}
