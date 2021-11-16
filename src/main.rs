use evdev::*;
use nix::{
    fcntl::{FcntlArg, OFlag},
    sys::epoll,
};
use std::os::unix::io::{AsRawFd, RawFd};

// The rest here is to ensure the epoll handle is cleaned up properly.
// You can also use the epoll crate, if you prefer.
struct Epoll(RawFd);

impl Epoll {
    pub(crate) fn new(fd: RawFd) -> Self {
        Epoll(fd)
    }
}

impl AsRawFd for Epoll {
    fn as_raw_fd(&self) -> RawFd {
        self.0
    }
}

impl Drop for Epoll {
    fn drop(&mut self) {
        let _ = nix::unistd::close(self.0);
    }
}

fn set_non_blocking(d: Device, n: u64) -> Device2 {
    let raw_fd = d.as_raw_fd();
    // Set nonblocking
    nix::fcntl::fcntl(raw_fd, FcntlArg::F_SETFL(OFlag::O_NONBLOCK)).unwrap();

    // Create epoll handle and attach raw_fd
    let epoll_fd =
        Epoll::new(epoll::epoll_create1(epoll::EpollCreateFlags::EPOLL_CLOEXEC).unwrap());
    let mut event = epoll::EpollEvent::new(epoll::EpollFlags::EPOLLIN, n);
    epoll::epoll_ctl(
        epoll_fd.as_raw_fd(),
        epoll::EpollOp::EpollCtlAdd,
        raw_fd,
        Some(&mut event),
    )
    .unwrap();
    let events = [epoll::EpollEvent::empty(); 2];

    Device2 {
        d,
        epoll_fd,
        event,
        events,
    }
}

struct Device2 {
    d: Device,
    epoll_fd: Epoll,
    event: epoll::EpollEvent,
    events: [epoll::EpollEvent; 2],
}

fn fetch(d: &mut Device2) -> Option<FetchEventsSynced> {
    match d.d.fetch_events() {
        Ok(iterator) => Some(iterator),
        Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
            // Wait forever for bytes available on raw_fd
            // optozorax: why you need that?
            // epoll::epoll_wait(d.epoll_fd.as_raw_fd(), &mut d.events, -1).ok()?;
            None
        }
        Err(e) => {
            eprintln!("{}", e);
            None
        }
    }
}

fn main() {
    let mut devices = evdev::enumerate()
        .enumerate()
        .filter(|(_, x)| x.supported_keys().is_some())
        .filter(|(i, _)| *i != 16)
        .map(|(i, x)| {
            (
                i,
                x.name().unwrap_or("unnamed").to_owned(),
                set_non_blocking(x, i as u64),
            )
        })
        .collect::<Vec<_>>();

    loop {
        for (_, name, d) in &mut devices {
            if let Some(iter) = fetch(d) {
                for ev in iter {
                    use InputEventKind::*;
                    match ev.kind() {
                        Key(x) => println!("{}: key: {:?} {}", name, x, ev.value()),
                        RelAxis(x) => println!("{}: relative axis: {:?} {}", name, x, ev.value()),
                        AbsAxis(x) => println!("{}: absolute axis: {:?} {}", name, x, ev.value()),
                        Misc(x) => {
                            if x != MiscType::MSC_SCAN {
                                println!("{}: misc: {:?} {}", name, x, ev.value());
                            }
                        }
                        Switch(x) => println!("{}: switch: {:?} {}", name, x, ev.value()),
                        Led(x) => println!("{}: led: {:?} {}", name, x, ev.value()),
                        Sound(x) => println!("{}: sound: {:?} {}", name, x, ev.value()),
                        Other => println!("{}: other {}", name, ev.value()),

                        // ignore these
                        Synchronization(_) => {}
                    }
                }
            }
        }
    }
}
