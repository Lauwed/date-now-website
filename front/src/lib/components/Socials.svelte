<script lang="ts">
	import Button from './Button.svelte';
</script>

<ul class="socials">
	<li class="socials__item"><Button variant="primary">Subscribe Now!</Button></li>

	<li class="socials__item">
		<Button tag="a" href="www.twitch.tv/devgirl_" title="Twitch">
			<svg
				xmlns="http://www.w3.org/2000/svg"
				viewBox="0 0 24 24"
				fill="none"
				stroke="currentColor"
				stroke-width="2"
				stroke-linecap="round"
				stroke-linejoin="round"><path d="M21 2H3v16h5v4l4-4h5l4-4V2zm-10 9V7m5 4V7" /></svg
			>
		</Button>
	</li>
	<li class="socials__item">
		<Button tag="a" href="" title="Instagram">
			<svg
				xmlns="http://www.w3.org/2000/svg"
				width="24"
				height="24"
				viewBox="0 0 24 24"
				fill="none"
				stroke="currentColor"
				stroke-width="2"
				stroke-linecap="round"
				stroke-linejoin="round"
				><rect width="20" height="20" x="2" y="2" rx="5" ry="5" /><path
					d="M16 11.37A4 4 0 1 1 12.63 8 4 4 0 0 1 16 11.37z"
				/><line x1="17.5" x2="17.51" y1="6.5" y2="6.5" /></svg
			>
		</Button>
	</li>
</ul>

<style lang="scss">
	.socials {
		display: flex;
		align-items: center;
		gap: 8px;

		&__item {
			svg {
				width: 18px;
			}
		}
	}
</style>
