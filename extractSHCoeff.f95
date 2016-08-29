
subroutine ExtractSHCoeff(DATA, N, L_MAX, HARM)
  implicit none

  interface
    subroutine SHExpandDH(grid, n, cilm, lmax, norm, sampling, csphase, lmax_calc)
      real*8, intent(in) :: grid(:,:)
      real*8, intent(out) :: cilm(:,:,:)
      integer, intent(in) :: n
      integer, intent(out) :: lmax
      integer, intent(in), optional :: norm, sampling, csphase, lmax_calc
    end subroutine SHExpandDH
  end interface

  integer, intent(in) :: N
  real*8, intent(in) :: DATA(N,2*N)
  integer, intent(in) :: L_MAX
  real*8, intent(out) :: HARM(2,L_MAX+1,L_MAX+1)
  integer :: L_MAX_RETURN
  integer :: l,m

  call SHExpandDH (grid=DATA, n=N, cilm=HARM, lmax=L_MAX_RETURN, lmax_calc=L_MAX, sampling=2)
  !
  ! do l=1,l_max+1
  !   do m=1,l_max+1
  !     write(*,*) HARM(1,l,m)
  !   end do
  ! end do
  ! !
  ! write(*,*) 'SIN:'
  !
  ! do l=1,l_max+1
  !   do m=1,l_max+1
  !     write(*,*) HARM(2,l,m)
  !   end do
  ! end do

  return
end subroutine ExtractSHCoeff