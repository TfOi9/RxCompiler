struct Borrowed<'a> {
    value: &'a i32,
}

fn choose<'a: 'a>(left: &'a i32, right: &'a i32, take_left: bool) -> &'a i32
where
    'a: 'a,
    &'a i32: 'a,
{
    if take_left { left } else { right }
}

impl<'a> Borrowed<'a>
where
    'a: 'a,
{
    fn get(&self) -> &'a i32 {
        self.value
    }
}

fn nested_reference<'a>(value: &&'a i32) -> &'a i32 {
    *value
}

fn static_reference(value: &'static i32) -> &'static i32 {
    value
}

fn parenthesized_type(value: (&'static i32)) -> (&'static i32) {
    value
}

fn borrow_mutably<'a>(value: &'a mut i32) -> &'a mut i32 {
    &mut *value
}
